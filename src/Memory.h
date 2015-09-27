#ifndef __MEMORY_H
#define __MEMORY_H

#include "Config.h"
#include "DRAM.h"
#include "Request.h"
#include "Controller.h"
#include "Statistics.h"
#include "GDDR5.h"
#include "HBM.h"
#include "LPDDR3.h"
#include "LPDDR4.h"
#include "WideIO2.h"
#include "DSARP.h"
#include <vector>
#include <functional>
#include <cmath>
#include <cassert>
#include <tuple>

using namespace std;

namespace ramulator
{

class MemoryBase{
public:
    MemoryBase() {}
    virtual ~MemoryBase() {}
    virtual double clk_ns() = 0;
    virtual void tick() = 0;
    virtual bool send(Request req) = 0;
    virtual int pending_requests() = 0;
    virtual void finish(void) = 0;
};

template <class T, template<typename> class Controller = Controller >
class Memory : public MemoryBase
{
protected:
  ScalarStat dram_capacity;
  ScalarStat num_dram_cycles;
  ScalarStat num_incoming_requests;
  ScalarStat num_read_requests;
  ScalarStat num_write_requests;
  ScalarStat ramulator_active_cycles;
  ScalarStat ramulator_refresh_cycles;
  ScalarStat ramulator_busy_cycles;
  VectorStat incoming_requests_per_channel;

  ScalarStat physical_page_replacement;
  ScalarStat maximum_bandwidth;
  ScalarStat in_queue_req_num_sum;
  ScalarStat in_queue_read_req_num_sum;
  ScalarStat in_queue_write_req_num_sum;

  long max_address;
public:
    enum class Type {
        ChRaBaRoCo,
        RoBaRaCoCh,
        MAX,
    } type = Type::RoBaRaCoCh;

    vector<Controller<T>*> ctrls;
    T * spec;
    vector<int> addr_bits;

    int tx_bits;

    Memory(const Config& configs, vector<Controller<T>*> ctrls)
        : ctrls(ctrls),
          spec(ctrls[0]->channel->spec),
          addr_bits(int(T::Level::MAX))
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
        // If hi address bits will not be assigned to Rows
        // then the chips must not be LPDDRx 6Gb, 12Gb etc.
        if (type != Type::RoBaRaCoCh && spec->standard_name.substr(0, 5) == "LPDDR")
            assert((sz[int(T::Level::Row)] & (sz[int(T::Level::Row)] - 1)) == 0);

        max_address = spec->channel_width / 8;

        for (unsigned int lev = 0; lev < addr_bits.size(); lev++) {
          addr_bits[lev] = calc_log2(sz[lev]);
            max_address *= sz[lev];
        }

        addr_bits[int(T::Level::MAX) - 1] -= calc_log2(spec->prefetch_size);

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
        num_incoming_requests
            .name("incoming_requests")
            .desc("Number of incoming requests to DRAM")
            .precision(0)
            ;
        num_read_requests
            .name("read_requests")
            .desc("Number of incoming read requests to DRAM")
            .precision(0)
            ;
        num_write_requests
            .name("write_requests")
            .desc("Number of incoming write requests to DRAM")
            .precision(0)
            ;
        incoming_requests_per_channel
            .init(sz[int(T::Level::Channel)])
            .name("incoming_requests_per_channel")
            .desc("Number of incoming requests to each DRAM channel")
            ;
        ramulator_active_cycles
            .name("ramulator_active_cycles")
            .desc("The total number of cycles that the DRAM part is active (serving R/W)")
            .precision(0)
            ;
        ramulator_refresh_cycles
            .name("ramulator_refresh_cycles")
            .desc("The total number of cycles that the DRAM part is under refresh")
            .precision(0)
            ;
        ramulator_busy_cycles
            .name("ramulator_busy_cycles")
            .desc("The total number of cycles that the DRAM part is active or under refresh")
            .precision(0)
            ;
        physical_page_replacement
            .name("physical_page_replacement")
            .desc("The number of times that physical page replacement happens.")
            .precision(0)
            ;
        maximum_bandwidth
            .name("maximum_bandwidth")
            .desc("The theoretical maximum bandwidth (Bps)")
            .precision(0)
            ;
        in_queue_req_num_sum
            .name("in_queue_req_num_sum")
            .desc("Sum of read/write queue length")
            .precision(0)
            ;
        in_queue_read_req_num_sum
            .name("in_queue_read_req_num_sum")
            .desc("Sum of read queue length")
            .precision(0)
            ;
        in_queue_write_req_num_sum
            .name("in_queue_write_req_num_sum")
            .desc("Sum of write queue length")
            .precision(0)
            ;

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

    void tick()
    {
        ++num_dram_cycles;

        bool is_active = false;
        bool is_refresh = false;
        for (auto ctrl : ctrls) {
          is_active = is_active || ctrl->is_active();
          is_refresh = is_refresh || ctrl->is_refresh();
          ctrl->tick();
        }
        if (is_active) {
          ramulator_active_cycles++;
        }
        if (is_refresh) {
          ramulator_refresh_cycles++;
        }
        if (is_active || is_refresh) {
          ramulator_busy_cycles++;
        }
        int cur_req_num = 0;
        int cur_que_req_num = 0;
        int cur_que_readreq_num = 0;
        int cur_que_writereq_num = 0;
        for (auto ctrl : ctrls) {
          cur_req_num += ctrl->channel->cur_serving_requests;
          cur_que_req_num += ctrl->readq.size() + ctrl->writeq.size();
          cur_que_readreq_num += ctrl->readq.size();
          cur_que_writereq_num += ctrl->writeq.size();
        }
        in_queue_req_num_sum += cur_que_req_num;
        in_queue_read_req_num_sum += cur_que_readreq_num;
        in_queue_write_req_num_sum += cur_que_writereq_num;
    }

    bool send(Request req)
    {
        req.addr_vec.resize(addr_bits.size());
        long addr = req.addr;
        // Each transaction size is 2^tx_bits, so first clear the lowest tx_bits bits
        clear_lower_bits(addr, tx_bits);

        switch(int(type)){
            case int(Type::ChRaBaRoCo):
                for (int i = addr_bits.size() - 1; i >= 0; i--)
                    req.addr_vec[i] = slice_lower_bits(addr, addr_bits[i]);
                break;
            case int(Type::RoBaRaCoCh):
                req.addr_vec[0] = slice_lower_bits(addr, addr_bits[0]);
                req.addr_vec[addr_bits.size() - 1] = slice_lower_bits(addr, addr_bits[addr_bits.size() - 1]);
                for (int i = 1; i <= int(T::Level::Row); i++)
                    req.addr_vec[i] = slice_lower_bits(addr, addr_bits[i]);
                break;
            default:
                assert(false);
        }

        if(ctrls[req.addr_vec[0]]->enqueue(req)) {
            // tally stats here to avoid double counting for requests that aren't enqueued
            ++num_incoming_requests;
            if (req.type == Request::Type::READ) {
              ++num_read_requests;
            }
            if (req.type == Request::Type::WRITE) {
              ++num_write_requests;
            }
            ++incoming_requests_per_channel[req.addr_vec[int(T::Level::Channel)]];
            return true;
        }

        return false;
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
      maximum_bandwidth = spec->speed_entry.rate * 1e6 * spec->channel_width * sz[int(T::Level::Channel)] / 8;
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
};

} /*namespace ramulator*/

#endif /*__MEMORY_H*/
