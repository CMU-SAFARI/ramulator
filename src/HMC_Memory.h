#ifndef __HMC_MEMORY_H
#define __HMC_MEMORY_H

#include "HMC.h"
#include "LogicLayer.h"
#include "Memory.h"
#include "Packet.h"

using namespace std;

namespace ramulator
{

template<>
class Memory<HMC, Controller> : public MemoryBase
{
protected:
  long max_address;
  long capacity_per_stack;
public:
    enum class Type {
        RoCoBaVaCo; // XXX The specification doesn't define row/column addressing
        MAX,
    } type = Type::RoCoBaVaCo;

    enum class Translation {
      None,
      Random,
      MAX,
    } translation = Translation::None;

    std::map<string, Translation> name_to_translation = {
      {"None", Translation::None},
      {"Random", Translation::Random},
    };

    vector<int> free_physical_pages;
    long free_physical_pages_remaining;
    map<pair<int, long>, long> page_translation;

    vector<list<int>> tags_pools;

    vector<Controller<HMC>*> ctrls;
    vector<LogicLayer<HMC>*> logic_layers;
    HMC * spec;

    vector<int> addr_bits;

    int tx_bits;

    Memory(const Config& configs, vector<Controller<HMC>*> ctrls)
        : ctrls(ctrls),
          spec(ctrls[0]->channel->spec),
          addr_bits(int(HMC::Level::MAX))
    {
        // make sure 2^N channels/ranks
        // TODO support channel number that is not powers of 2
        int *sz = spec->org_entry.count;
        assert((sz[0] & (sz[0] - 1)) == 0);
        assert((sz[1] & (sz[1] - 1)) == 0);
        // validate size of one transaction
        int tx = (spec->prefetch_size * spec->channel_width / 8);
        tx_bits = calc_log2(tx);
        assert((1<<tx_bits) == tx);

        capacity_per_stack = spec->channel_width / 8;

        for (unsigned int lev = 0; lev < addr_bits.size(); lev++) {
          addr_bits[lev] = calc_log2(sz[lev]);
          capacity_per_stack *= sz[lev];
        }
        max_address = capacity_per_stack * configs.get_stacks();
        printf("max_address: %lx\n", max_address);
        printf("capacity_per_stack: %lx\n", capacity_per_stack);

        addr_bits[int(HMC::Level::MAX) - 1] -= calc_log2(spec->prefetch_size);

        // Initiating translation
        if (configs.contains("translation")) {
          translation = name_to_translation[configs["translation"]];
        }
        if (translation != Translation::None) {
          // construct a list of available pages
          // TODO: this should not assume a 4KB page!
          free_physical_pages_remaining = max_address >> 12;

          free_physical_pages.resize(free_physical_pages_remaining, -1);
        }

        assert(spec->source_links > 0);
        tags_pools.resize(spec->source_links);
        for (auto tags_pool : tags_pools) {
          for (int i = 0 ; i < spec->max_tags ; ++i) {
            tags_pool.push_back(i);
          }
        }
    }

    ~Memory()
    {
        for (auto ctrl: ctrls)
            delete ctrl;
        delete spec;
    }

    double clk_ns()
    {
        return spec->speed_entry.tCK;
    }

    void record_core(int coreid) {
      record_read_requests[coreid] = num_read_requests[coreid];
      record_write_requests[coreid] = num_write_requests[coreid];
      for (auto ctrl : ctrls) {
        ctrl->record_core(coreid);
      }
    }

    void tick()
    {
        for (auto ctrl : ctrls) {
          ctrl->tick();
        }
        for (auto logic_layer : logic_layers) {
          logic_layer->tick();
        }
    }

    int assign_tag(int slid) {
      if (tags_pools[slid].empty()) {
        return -1;
      } else {
        int tag = tags_pools[slid].front();
        tags_pools[slid].pop_front();
        return tag;
      }
    }

    Packet form_request_packet(const Request& req) {
      // All packets sent from host controller are Request packets
      int cub = req.addr / capacity_per_stack;
      int adrs = req.addr;
      int slid = max_address & ((1 << spec->source_links) - 1) // max_address % spec->source_links
      int tag = assign_tag(SLID); // may return -1 when no available tag // TODO recycle tags when request callback
      int lng = spec->payload_flits;
      Packet::Command cmd;
      switch (int(req.type)) {
        case int(Request::Type::READ):
          cmd = read_cmd_map[lng];
        break;
        case int(Request::Type::WRITE):
          cmd = write_cmd_map[lng];
        break;
        default: assert(false);
      }
      Packet packet(Packet::Type::Request, cub, adrs, tag, lng, slid, cmd);
      packet.req = req;
      // DEBUG:
      assert(packet.header.CUB.valid());
      assert(packet.header.ADRS.valid());
      assert(packet.header.TAG.valid()); // -1 also considered valid here...
      assert(packet.tail.SLID.valid());
      assert(packet.header.CMD.valid());
      return packet;
    }

    void receive(Packet& packet) {
      assert(packet.type == Request::Type::Response);
      tags_pools[packet.header.slid.value].push_back(packet.header.tag.value);
      Request& req = packet.req;
      req.callback(req);
    }

    bool send(Request req)
    {
        req.addr_vec.resize(addr_bits.size());
        long addr = req.addr;
        int coreid = req.coreid;

        // Each transaction size is 2^tx_bits, so first clear the lowest tx_bits bits
        clear_lower_bits(addr, tx_bits);

        switch(int(type)) {
          case int(Type::RoCoBaVaCo):
            int max_block_bits = spec->maxblock_entry.flit_num_bits;
            req.addr_vec[int(HMC::Level::Column)] =
                slice_lower_bits(addr, max_block_bits);
            req.addr_vec[int(HMC::Level::Vault)] =
                slice_lower_bits(addr, addr_bits[int(HMC::Level::Vault)]);
            req.addr_vec[int(HMC::Level::Bank)] =
                slice_lower_bits(addr, addr_bits[int(HMC::Level::Bank)]);
            int column_MSB_bits =
              slice_lower_bits(addr, addr_bits[int(HMC::Level::Column)] - max_block_bits);
            req.addr_vec[int(HMC::Level::Column)] =
              req.addr_vec[int(HMC::Level::Column)] | (column_MSB_bits << max_block_bits);
            req.addr_vec[int(HMC::Level::Row)] =
                slice_lower_bits(addr, addr_bits[int(HMC::Level::Row)]);
          default:
              assert(false);
        }

        Packet packet = form_request_packet(req);
        if (req.tag == -1) {
          printf("tag for link %d not available\n", req.tail.slid.value);
          return false;
        }

        Link* link = links[req.tail.slid];
        if (packet.total_flits <= link->slave.available_space()) {
          link->slave.receive(packet);
          return true;
        } else {
          return false;
        }
    }

    int pending_requests()
    {
        int reqs = 0;
        for (auto ctrl: ctrls)
            reqs += ctrl->readq.size() + ctrl->writeq.size() + ctrl->otherq.size() + ctrl->pending.size();
        return reqs;
    }

private:

    int calc_log2(int val){
        int n = 0;
        while ((val >>= 1))
            n ++;
        return n;
    }
    int slice_lower_bits(long& addr, int bits)
    {
        int lbits = addr & ((1<<bits) - 1);
        addr >>= bits;
        return lbits;
    }
    void clear_lower_bits(long& addr, int bits)
    {
        addr >>= bits;
    }
    long lrand(void) {
        if(sizeof(int) < sizeof(long)) {
            return static_cast<long>(rand()) << (sizeof(int) * 8) | rand();
        }

        return rand();
    }
};

} /*namespace ramulator*/

#endif /*__HMC_MEMORY_H*/
