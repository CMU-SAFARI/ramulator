#ifndef __HMC_MEMORY_H
#define __HMC_MEMORY_H

#include "HMC.h"
#include "LogicLayer.h"
#include "LogicLayer.cpp"
#include "Memory.h"
#include "Packet.h"
#include "Statistics.h"

using namespace std;

namespace ramulator
{

template<>
class Memory<HMC, Controller> : public MemoryBase
{
protected:
  long max_address;
  long capacity_per_stack;
  ScalarStat dram_capacity;
  ScalarStat num_dram_cycles;
  VectorStat num_read_requests;
  VectorStat num_write_requests;
  ScalarStat ramulator_active_cycles;
  ScalarStat memory_footprint;
  VectorStat incoming_requests_per_channel;
  VectorStat incoming_read_reqs_per_channel;
  ScalarStat physical_page_replacement;
  ScalarStat maximum_internal_bandwidth;
  ScalarStat maximum_link_bandwidth;
  ScalarStat read_bandwidth;
  ScalarStat write_bandwidth;

  ScalarStat read_latency_avg;
  ScalarStat read_latency_ns_avg;
  ScalarStat read_latency_sum;
  ScalarStat queueing_latency_avg;
  ScalarStat queueing_latency_ns_avg;
  ScalarStat queueing_latency_sum;
  ScalarStat request_packet_latency_avg;
  ScalarStat request_packet_latency_ns_avg;
  ScalarStat request_packet_latency_sum;
  ScalarStat response_packet_latency_avg;
  ScalarStat response_packet_latency_ns_avg;
  ScalarStat response_packet_latency_sum;

  // shared by all Controller objects
  ScalarStat read_transaction_bytes;
  ScalarStat write_transaction_bytes;
  ScalarStat row_hits;
  ScalarStat row_misses;
  ScalarStat row_conflicts;
  VectorStat read_row_hits;
  VectorStat read_row_misses;
  VectorStat read_row_conflicts;
  VectorStat write_row_hits;
  VectorStat write_row_misses;
  VectorStat write_row_conflicts;

  ScalarStat req_queue_length_avg;
  ScalarStat req_queue_length_sum;
  ScalarStat read_req_queue_length_avg;
  ScalarStat read_req_queue_length_sum;
  ScalarStat write_req_queue_length_avg;
  ScalarStat write_req_queue_length_sum;

  VectorStat record_read_hits;
  VectorStat record_read_misses;
  VectorStat record_read_conflicts;
  VectorStat record_write_hits;
  VectorStat record_write_misses;
  VectorStat record_write_conflicts;

  long mem_req_count = 0;

public:
    long clk = 0;
    enum class Type {
        RoCoBaVa, // XXX The specification doesn't define row/column addressing
        RoBaCoVa,
        RoCoBaBgVa,
        MAX,
    } type = Type::RoCoBaVa;

    std::map<std::string, Type> name_to_type = {
      {"RoCoBaVa", Type::RoCoBaVa},
      {"RoBaCoVa", Type::RoBaCoVa},
      {"RoCoBaBgVa", Type::RoCoBaBgVa}};

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

        // Initiating addressing
        if (configs.contains("addressing_type")) {
          assert(name_to_type.find(configs["addressing_type"]) != name_to_type.end());
          printf("configs[\"addressing_type\"] %s\n", configs["addressing_type"].c_str());
          type = name_to_type[configs["addressing_type"]];
        }

        // HMC
        assert(spec->source_links > 0);
        tags_pools.resize(spec->source_links);
        for (auto & tags_pool : tags_pools) {
          for (int i = 0 ; i < spec->max_tags ; ++i) {
            tags_pool.push_back(i);
          }
        }

        int stacks = configs.get_int_value("stacks");
        for (int i = 0 ; i < stacks ; ++i) {
          logic_layers.emplace_back(new LogicLayer<HMC>(configs, i, spec, ctrls,
              this, std::bind(&Memory<HMC>::receive_packets, this,
                              std::placeholders::_1)));
        }

        // regStats
        dram_capacity
            .name("dram_capacity")
            .desc("Number of bytes in simulated DRAM")
            .precision(0)
            ;
        dram_capacity = max_address;

        num_dram_cycles
            .name("dram_cycles")
            .desc("Number of DRAM cycles simulated")
            .precision(0)
            ;

        num_read_requests
            .init(configs.get_core_num())
            .name("read_requests")
            .desc("Number of incoming read requests to DRAM")
            .precision(0)
            ;

        num_write_requests
            .init(configs.get_core_num())
            .name("write_requests")
            .desc("Number of incoming write requests to DRAM")
            .precision(0)
            ;

        incoming_requests_per_channel
            .init(sz[int(HMC::Level::Vault)])
            .name("incoming_requests_per_channel")
            .desc("Number of incoming requests to each DRAM channel")
            .precision(0)
            ;

        incoming_read_reqs_per_channel
            .init(sz[int(HMC::Level::Vault)])
            .name("incoming_read_reqs_per_channel")
            .desc("Number of incoming read requests to each DRAM channel")
            .precision(0)
            ;
        ramulator_active_cycles
            .name("ramulator_active_cycles")
            .desc("The total number of cycles that the DRAM part is active (serving R/W)")
            .precision(0)
            ;
        memory_footprint
            .name("memory_footprint")
            .desc("memory footprint in byte")
            .precision(0)
            ;
        physical_page_replacement
            .name("physical_page_replacement")
            .desc("The number of times that physical page replacement happens.")
            .precision(0)
            ;

        maximum_internal_bandwidth
            .name("maximum_internal_bandwidth")
            .desc("The theoretical maximum bandwidth (Bps)")
            .precision(0)
            ;

        maximum_link_bandwidth
            .name("maximum_link_bandwidth")
            .desc("The theoretical maximum bandwidth (Bps)")
            .precision(0)
            ;

        read_bandwidth
            .name("read_bandwidth")
            .desc("Real read bandwidth(Bps)")
            .precision(0)
            ;

        write_bandwidth
            .name("write_bandwidth")
            .desc("Real write bandwidth(Bps)")
            .precision(0)
            ;
        read_latency_sum
            .name("read_latency_sum")
            .desc("The memory latency cycles (in memory time domain) sum for all read requests")
            .precision(0)
            ;
        read_latency_avg
            .name("read_latency_avg")
            .desc("The average memory latency cycles (in memory time domain) per request for all read requests")
            .precision(6)
            ;
        queueing_latency_sum
            .name("queueing_latency_sum")
            .desc("The sum of time waiting in queue before first command issued")
            .precision(0)
            ;
        queueing_latency_avg
            .name("queueing_latency_avg")
            .desc("The average of time waiting in queue before first command issued")
            .precision(6)
            ;
        read_latency_ns_avg
            .name("read_latency_ns_avg")
            .desc("The average memory latency (ns) per request for all read requests in this channel")
            .precision(6)
            ;
        queueing_latency_ns_avg
            .name("queueing_latency_ns_avg")
            .desc("The average of time (ns) waiting in queue before first command issued")
            .precision(6)
            ;
        request_packet_latency_sum
            .name("request_packet_latency_sum")
            .desc("The memory latency cycles (in memory time domain) sum for all read request packets transmission")
            .precision(0)
            ;
        request_packet_latency_avg
            .name("request_packet_latency_avg")
            .desc("The average memory latency cycles (in memory time domain) per request for all read request packets transmission")
            .precision(6)
            ;
        request_packet_latency_ns_avg
            .name("request_packet_latency_ns_avg")
            .desc("The average memory latency (ns) per request for all read request packets transmission")
            .precision(6)
            ;
        response_packet_latency_sum
            .name("response_packet_latency_sum")
            .desc("The memory latency cycles (in memory time domain) sum for all read response packets transmission")
            .precision(0)
            ;
        response_packet_latency_avg
            .name("response_packet_latency_avg")
            .desc("The average memory latency cycles (in memory time domain) per response for all read response packets transmission")
            .precision(6)
            ;
        response_packet_latency_ns_avg
            .name("response_packet_latency_ns_avg")
            .desc("The average memory latency (ns) per response for all read response packets transmission")
            .precision(6)
            ;

        // shared by all Controller objects

        read_transaction_bytes
            .name("read_transaction_bytes")
            .desc("The total byte of read transaction")
            .precision(0)
            ;
        write_transaction_bytes
            .name("write_transaction_bytes")
            .desc("The total byte of write transaction")
            .precision(0)
            ;

        row_hits
            .name("row_hits")
            .desc("Number of row hits")
            .precision(0)
            ;
        row_misses
            .name("row_misses")
            .desc("Number of row misses")
            .precision(0)
            ;
        row_conflicts
            .name("row_conflicts")
            .desc("Number of row conflicts")
            .precision(0)
            ;

        read_row_hits
            .init(configs.get_core_num())
            .name("read_row_hits")
            .desc("Number of row hits for read requests")
            .precision(0)
            ;
        read_row_misses
            .init(configs.get_core_num())
            .name("read_row_misses")
            .desc("Number of row misses for read requests")
            .precision(0)
            ;
        read_row_conflicts
            .init(configs.get_core_num())
            .name("read_row_conflicts")
            .desc("Number of row conflicts for read requests")
            .precision(0)
            ;

        write_row_hits
            .init(configs.get_core_num())
            .name("write_row_hits")
            .desc("Number of row hits for write requests")
            .precision(0)
            ;
        write_row_misses
            .init(configs.get_core_num())
            .name("write_row_misses")
            .desc("Number of row misses for write requests")
            .precision(0)
            ;
        write_row_conflicts
            .init(configs.get_core_num())
            .name("write_row_conflicts")
            .desc("Number of row conflicts for write requests")
            .precision(0)
            ;

        req_queue_length_sum
            .name("req_queue_length_sum")
            .desc("Sum of read and write queue length per memory cycle.")
            .precision(0)
            ;
        req_queue_length_avg
            .name("req_queue_length_avg")
            .desc("Average of read and write queue length per memory cycle.")
            .precision(6)
            ;

        read_req_queue_length_sum
            .name("read_req_queue_length_sum")
            .desc("Read queue length sum per memory cycle.")
            .precision(0)
            ;
        read_req_queue_length_avg
            .name("read_req_queue_length_avg")
            .desc("Read queue length average per memory cycle.")
            .precision(6)
            ;

        write_req_queue_length_sum
            .name("write_req_queue_length_sum")
            .desc("Write queue length sum per memory cycle.")
            .precision(0)
            ;
        write_req_queue_length_avg
            .name("write_req_queue_length_avg")
            .desc("Write queue length average per memory cycle.")
            .precision(6)
            ;

        record_read_hits
            .init(configs.get_core_num())
            .name("record_read_hits")
            .desc("record read hit count for this core when it reaches request limit or to the end")
            ;

        record_read_misses
            .init(configs.get_core_num())
            .name("record_read_misses")
            .desc("record_read_miss count for this core when it reaches request limit or to the end")
            ;

        record_read_conflicts
            .init(configs.get_core_num())
            .name("record_read_conflicts")
            .desc("record read conflict count for this core when it reaches request limit or to the end")
            ;

        record_write_hits
            .init(configs.get_core_num())
            .name("record_write_hits")
            .desc("record write hit count for this core when it reaches request limit or to the end")
            ;

        record_write_misses
            .init(configs.get_core_num())
            .name("record_write_misses")
            .desc("record write miss count for this core when it reaches request limit or to the end")
            ;

        record_write_conflicts
            .init(configs.get_core_num())
            .name("record_write_conflicts")
            .desc("record write conflict for this core when it reaches request limit or to the end")
            ;

        for (auto ctrl : ctrls) {
          ctrl->read_transaction_bytes = &read_transaction_bytes;
          ctrl->write_transaction_bytes = &write_transaction_bytes;

          ctrl->row_hits = &row_hits;
          ctrl->row_misses = &row_misses;
          ctrl->row_conflicts = &row_conflicts;
          ctrl->read_row_hits = &read_row_hits;
          ctrl->read_row_misses = &read_row_misses;
          ctrl->read_row_conflicts = &read_row_conflicts;
          ctrl->write_row_hits = &write_row_hits;
          ctrl->write_row_misses = &write_row_misses;
          ctrl->write_row_conflicts = &write_row_conflicts;

          ctrl->queueing_latency_sum = &queueing_latency_sum;

          ctrl->req_queue_length_sum = &req_queue_length_sum;
          ctrl->read_req_queue_length_sum = &read_req_queue_length_sum;
          ctrl->write_req_queue_length_sum = &write_req_queue_length_sum;

          ctrl->record_read_hits = &record_read_hits;
          ctrl->record_read_misses = &record_read_misses;
          ctrl->record_read_conflicts = &record_read_conflicts;
          ctrl->record_write_hits = &record_write_hits;
          ctrl->record_write_misses = &record_write_misses;
          ctrl->record_write_conflicts = &record_write_conflicts;
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
      // TODO record multicore statistics
    }

    void tick()
    {
        clk++;
        num_dram_cycles++;

        bool is_active = false;
        for (auto ctrl : ctrls) {
          is_active = is_active || ctrl->is_active();
          ctrl->tick();
        }
        if (is_active) {
          ramulator_active_cycles++;
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
      long addr = req.addr;
      int cub = addr / capacity_per_stack;
      long adrs = addr;
      int max_block_bits = spec->maxblock_entry.flit_num_bits;
      clear_lower_bits(addr, max_block_bits);
      int slid = addr % spec->source_links;
      int tag = assign_tag(slid); // may return -1 when no available tag // TODO recycle tags when request callback
      int lng = req.type == Request::Type::READ ?
                                                1 : 1 +  spec->payload_flits;
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
      Packet packet(Packet::Type::REQUEST, cub, adrs, tag, lng, slid, cmd);
      packet.req = req;
      debug_hmc("cub: %d", cub);
      debug_hmc("adrs: %lx", adrs);
      debug_hmc("slid: %d", slid);
      debug_hmc("lng: %d", lng);
      debug_hmc("cmd: %d", int(cmd));
      // DEBUG:
      assert(packet.header.CUB.valid());
      assert(packet.header.ADRS.valid());
      assert(packet.header.TAG.valid()); // -1 also considered valid here...
      assert(packet.tail.SLID.valid());
      assert(packet.header.CMD.valid());
      return packet;
    }

    void receive_packets(Packet packet) {
      debug_hmc("receive response packets@host controller");
      if (packet.flow_control) {
        return;
      }
      assert(packet.type == Packet::Type::RESPONSE);
      tags_pools[packet.header.SLID.value].push_back(packet.header.TAG.value);
      Request& req = packet.req;
      req.depart_hmc = clk;
      if (req.type == Request::Type::READ) {
        read_latency_sum += req.depart_hmc - req.arrive_hmc;
        debug_hmc("read_latency: %ld", req.depart_hmc - req.arrive_hmc);
        request_packet_latency_sum += req.arrive - req.arrive_hmc;
        debug_hmc("request_packet_latency: %ld", req.arrive - req.arrive_hmc);
        response_packet_latency_sum += req.depart_hmc - req.depart;
        debug_hmc("response_packet_latency: %ld", req.depart_hmc - req.depart);
        req.callback(req);
      }
    }

    bool send(Request req)
    {
        debug_hmc("receive request packets@host controller");
        req.addr_vec.resize(addr_bits.size());
        req.reqid = mem_req_count;
        clear_higher_bits(req.addr, max_address-1ll);
        long addr = req.addr;
        long coreid = req.coreid;

        // Each transaction size is 2^tx_bits, so first clear the lowest tx_bits bits
        clear_lower_bits(addr, tx_bits);

        switch(int(type)) {
          case int(Type::RoCoBaVa): {
            int max_block_col_bits =
                spec->maxblock_entry.flit_num_bits - tx_bits;
            req.addr_vec[int(HMC::Level::Column)] =
                slice_lower_bits(addr, max_block_col_bits);
            req.addr_vec[int(HMC::Level::Vault)] =
                slice_lower_bits(addr, addr_bits[int(HMC::Level::Vault)]);
            req.addr_vec[int(HMC::Level::Bank)] =
                slice_lower_bits(addr, addr_bits[int(HMC::Level::Bank)]);
            req.addr_vec[int(HMC::Level::BankGroup)] =
                slice_lower_bits(addr, addr_bits[int(HMC::Level::BankGroup)]);
            int column_MSB_bits =
              slice_lower_bits(
                  addr, addr_bits[int(HMC::Level::Column)] - max_block_col_bits);
            req.addr_vec[int(HMC::Level::Column)] =
              req.addr_vec[int(HMC::Level::Column)] | (column_MSB_bits << max_block_col_bits);
            req.addr_vec[int(HMC::Level::Row)] =
                slice_lower_bits(addr, addr_bits[int(HMC::Level::Row)]);
          }
          break;
          case int(Type::RoBaCoVa): {
            int max_block_col_bits =
                spec->maxblock_entry.flit_num_bits - tx_bits;
            req.addr_vec[int(HMC::Level::Column)] =
                slice_lower_bits(addr, max_block_col_bits);
            req.addr_vec[int(HMC::Level::Vault)] =
                slice_lower_bits(addr, addr_bits[int(HMC::Level::Vault)]);
            int column_MSB_bits =
              slice_lower_bits(
                  addr, addr_bits[int(HMC::Level::Column)] - max_block_col_bits);
            req.addr_vec[int(HMC::Level::Column)] =
              req.addr_vec[int(HMC::Level::Column)] | (column_MSB_bits << max_block_col_bits);
            req.addr_vec[int(HMC::Level::Bank)] =
                slice_lower_bits(addr, addr_bits[int(HMC::Level::Bank)]);
            req.addr_vec[int(HMC::Level::BankGroup)] =
                slice_lower_bits(addr, addr_bits[int(HMC::Level::BankGroup)]);
            req.addr_vec[int(HMC::Level::Row)] =
                slice_lower_bits(addr, addr_bits[int(HMC::Level::Row)]);
          }
          break;
          case int(Type::RoCoBaBgVa): {
            int max_block_col_bits =
                spec->maxblock_entry.flit_num_bits - tx_bits;
            req.addr_vec[int(HMC::Level::Column)] =
                slice_lower_bits(addr, max_block_col_bits);
            req.addr_vec[int(HMC::Level::Vault)] =
                slice_lower_bits(addr, addr_bits[int(HMC::Level::Vault)]);
            req.addr_vec[int(HMC::Level::BankGroup)] =
                slice_lower_bits(addr, addr_bits[int(HMC::Level::BankGroup)]);
            req.addr_vec[int(HMC::Level::Bank)] =
                slice_lower_bits(addr, addr_bits[int(HMC::Level::Bank)]);
            int column_MSB_bits =
              slice_lower_bits(
                  addr, addr_bits[int(HMC::Level::Column)] - max_block_col_bits);
            req.addr_vec[int(HMC::Level::Column)] =
              req.addr_vec[int(HMC::Level::Column)] | (column_MSB_bits << max_block_col_bits);
            req.addr_vec[int(HMC::Level::Row)] =
                slice_lower_bits(addr, addr_bits[int(HMC::Level::Row)]);
          }
          break;
          default:
              assert(false);
        }

        req.arrive_hmc = clk;
        Packet packet = form_request_packet(req);
        if (packet.header.TAG.value == -1) {
          debug_hmc("tag for link %d not available", packet.tail.SLID.value);
          return false;
        }

        // TODO support multiple stacks
        Link<HMC>* link =
            logic_layers[0]->host_links[packet.tail.SLID.value].get();
        if (packet.total_flits <= link->slave.available_space()) {
          link->slave.receive(packet);
          if (req.type == Request::Type::READ) {
            ++num_read_requests[coreid];
            ++incoming_read_reqs_per_channel[req.addr_vec[int(HMC::Level::Vault)]];
          }
          if (req.type == Request::Type::WRITE) {
            ++num_write_requests[coreid];
          }
          ++incoming_requests_per_channel[req.addr_vec[int(HMC::Level::Vault)]];
          ++mem_req_count;
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

    void finish(void) {
      dram_capacity = max_address;
      int *sz = spec->org_entry.count;
      maximum_internal_bandwidth =
        spec->speed_entry.rate * 1e6 * spec->channel_width * sz[int(HMC::Level::Vault)] / 8;
      maximum_link_bandwidth =
        spec->link_width * 2 * spec->source_links * spec->lane_speed * 1e9 / 8;

      long dram_cycles = num_dram_cycles.value();
      long total_read_req = num_read_requests.total();
      for (auto ctrl : ctrls) {
        ctrl->finish(dram_cycles);
      }
      read_bandwidth = read_transaction_bytes.value() * 1e9 / (dram_cycles * clk_ns());
      write_bandwidth = write_transaction_bytes.value() * 1e9 / (dram_cycles * clk_ns());;
      read_latency_avg = read_latency_sum.value() / total_read_req;
      queueing_latency_avg = queueing_latency_sum.value() / total_read_req;
      request_packet_latency_avg = request_packet_latency_sum.value() / total_read_req;
      response_packet_latency_avg = response_packet_latency_sum.value() / total_read_req;
      read_latency_ns_avg = read_latency_avg.value() * clk_ns();
      queueing_latency_ns_avg = queueing_latency_avg.value() * clk_ns();
      request_packet_latency_ns_avg = request_packet_latency_avg.value() * clk_ns();
      response_packet_latency_ns_avg = response_packet_latency_avg.value() * clk_ns();
      req_queue_length_avg = req_queue_length_sum.value() / dram_cycles;
      read_req_queue_length_avg = read_req_queue_length_sum.value() / dram_cycles;
      write_req_queue_length_avg = write_req_queue_length_sum.value() / dram_cycles;
    }

    long page_allocator(long addr, int coreid) {
        long virtual_page_number = addr >> 12;

        switch(int(translation)) {
            case int(Translation::None): {
              return addr;
            }
            case int(Translation::Random): {
                auto target = make_pair(coreid, virtual_page_number);
                if(page_translation.find(target) == page_translation.end()) {
                    // page doesn't exist, so assign a new page
                    // make sure there are physical pages left to be assigned

                    // if physical page doesn't remain, replace a previous assigned
                    // physical page.
                    memory_footprint += 1<<12;
                    if (!free_physical_pages_remaining) {
                      physical_page_replacement++;
                      long phys_page_to_read = lrand() % free_physical_pages.size();
                      assert(free_physical_pages[phys_page_to_read] != -1);
                      page_translation[target] = phys_page_to_read;
                    } else {
                        // assign a new page
                        long phys_page_to_read = lrand() % free_physical_pages.size();
                        // if the randomly-selected page was already assigned
                        if(free_physical_pages[phys_page_to_read] != -1) {
                            long starting_page_of_search = phys_page_to_read;

                            do {
                                // iterate through the list until we find a free page
                                // TODO: does this introduce serious non-randomness?
                                ++phys_page_to_read;
                                phys_page_to_read %= free_physical_pages.size();
                            }
                            while((phys_page_to_read != starting_page_of_search) && free_physical_pages[phys_page_to_read] != -1);
                        }

                        assert(free_physical_pages[phys_page_to_read] == -1);

                        page_translation[target] = phys_page_to_read;
                        free_physical_pages[phys_page_to_read] = coreid;
                        --free_physical_pages_remaining;
                    }
                }

                // SAUGATA TODO: page size should not always be fixed to 4KB
                return (page_translation[target] << 12) | (addr & ((1 << 12) - 1));
            }
            default:
                assert(false);
        }

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
    void clear_higher_bits(long& addr, long mask) {
        addr = (addr & mask);
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
