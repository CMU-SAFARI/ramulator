#ifndef __MEMORY_H
#define __MEMORY_H

#include "Config.h"
#include "DRAM.h"
#include "Request.h"
#include "Controller.h"
#include "HMC_Controller.h"
#include "SpeedyController.h"
#include "Statistics.h"
#include "GDDR5.h"
#include "HBM.h"
#include "HMC.h"
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
    virtual long page_allocator(long addr, int coreid) = 0;
    virtual void record_core(int coreid) = 0;
};

template <class T, template<typename> class Controller = Controller >
class Memory : public MemoryBase
{
protected:
  ScalarStat dram_capacity;
  ScalarStat num_dram_cycles;
  ScalarStat num_incoming_requests;
  VectorStat num_read_requests;
  VectorStat num_write_requests;
  ScalarStat ramulator_active_cycles;
  ScalarStat memory_footprint;
  VectorStat incoming_requests_per_channel;
  VectorStat incoming_read_reqs_per_channel;

  ScalarStat physical_page_replacement;
  ScalarStat maximum_bandwidth;
  ScalarStat read_bandwidth;
  ScalarStat write_bandwidth;

#ifndef INTEGRATED_WITH_GEM5
  VectorStat record_read_requests;
  VectorStat record_write_requests;
#endif

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

  ScalarStat read_latency_avg;
  ScalarStat read_latency_ns_avg;
  ScalarStat read_latency_sum;
  ScalarStat queueing_latency_avg;
  ScalarStat queueing_latency_ns_avg;
  ScalarStat queueing_latency_sum;

  ScalarStat req_queue_length_avg;
  ScalarStat req_queue_length_sum;
  ScalarStat read_req_queue_length_avg;
  ScalarStat read_req_queue_length_sum;
  ScalarStat write_req_queue_length_avg;
  ScalarStat write_req_queue_length_sum;

#ifndef INTEGRATED_WITH_GEM5
  VectorStat record_read_hits;
  VectorStat record_read_misses;
  VectorStat record_read_conflicts;
  VectorStat record_write_hits;
  VectorStat record_write_misses;
  VectorStat record_write_conflicts;
#endif

  long max_address;
public:
    enum class Type {
        ChRaBaRoCo,
        RoBaRaCoCh,
        MAX,
    } type = Type::RoBaRaCoCh;

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

    vector<Controller<T>*> ctrls;
    T * spec;
    vector<int> addr_bits;

    int tx_bits;
    int cacheline_size;

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

        cacheline_size = configs.get_cacheline_size();

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
        num_incoming_requests
            .name("incoming_requests")
            .desc("Number of incoming requests to DRAM")
            .precision(0)
            ;
        num_read_requests
            .init(configs.get_core_num())
            .name("read_requests")
            .desc("Number of incoming read requests to DRAM per core")
            .precision(0)
            ;
        num_write_requests
            .init(configs.get_core_num())
            .name("write_requests")
            .desc("Number of incoming write requests to DRAM per core")
            .precision(0)
            ;
        incoming_requests_per_channel
            .init(sz[int(T::Level::Channel)])
            .name("incoming_requests_per_channel")
            .desc("Number of incoming requests to each DRAM channel")
            ;
        incoming_read_reqs_per_channel
            .init(sz[int(T::Level::Channel)])
            .name("incoming_read_reqs_per_channel")
            .desc("Number of incoming read requests to each DRAM channel")
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

        maximum_bandwidth
            .name("maximum_bandwidth")
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

#ifndef INTEGRATED_WITH_GEM5
        record_read_requests
            .init(configs.get_core_num())
            .name("record_read_requests")
            .desc("record read requests for this core when it reaches request limit or to the end")
            ;

        record_write_requests
            .init(configs.get_core_num())
            .name("record_write_requests")
            .desc("record write requests for this core when it reaches request limit or to the end")
            ;
#endif

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

        read_latency_sum
            .name("read_latency_sum")
            .desc("The memory latency cycles (in memory time domain) sum for all read requests in this channel")
            .precision(0)
            ;
        read_latency_avg
            .name("read_latency_avg")
            .desc("The average memory latency cycles (in memory time domain) per request for all read requests in this channel")
            .precision(6)
            ;
        queueing_latency_sum
            .name("queueing_latency_sum")
            .desc("The sum of cycles waiting in queue before first command issued")
            .precision(0)
            ;
        queueing_latency_avg
            .name("queueing_latency_avg")
            .desc("The average of cycles waiting in queue before first command issued")
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
#ifndef INTEGRATED_WITH_GEM5
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
#endif

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

          ctrl->read_latency_sum = &read_latency_sum;
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
#ifndef INTEGRATED_WITH_GEM5
      record_read_requests[coreid] = num_read_requests[coreid];
      record_write_requests[coreid] = num_write_requests[coreid];
#endif
      for (auto ctrl : ctrls) {
        ctrl->record_core(coreid);
      }
    }

    void tick()
    {
        ++num_dram_cycles;

        bool is_active = false;
        for (auto ctrl : ctrls) {
          is_active = is_active || ctrl->is_active();
          ctrl->tick();
        }
        if (is_active) {
          ramulator_active_cycles++;
        }
    }

    bool send(Request req)
    {
        req.addr_vec.resize(addr_bits.size());
        req.burst_count = cacheline_size / (1 << tx_bits);
        long addr = req.addr;
        int coreid = req.coreid;

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
              ++num_read_requests[coreid];
              ++incoming_read_reqs_per_channel[req.addr_vec[int(T::Level::Channel)]];
            }
            if (req.type == Request::Type::WRITE) {
              ++num_write_requests[coreid];
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

      long dram_cycles = num_dram_cycles.value();
      long total_read_req = num_read_requests.total();
      for (auto ctrl : ctrls) {
        ctrl->finish(dram_cycles);
      }
      read_bandwidth = read_transaction_bytes.value() * 1e9 / (dram_cycles * clk_ns());
      write_bandwidth = write_transaction_bytes.value() * 1e9 / (dram_cycles * clk_ns());
      read_latency_avg = read_latency_sum.value() / total_read_req;
      queueing_latency_avg = queueing_latency_sum.value() / total_read_req;
      read_latency_ns_avg = read_latency_avg.value() * clk_ns();
      queueing_latency_ns_avg = queueing_latency_avg.value() * clk_ns();
      req_queue_length_avg = req_queue_length_sum.value() / dram_cycles;
      read_req_queue_length_avg = read_req_queue_length_sum.value() / dram_cycles;
      write_req_queue_length_avg = write_req_queue_length_sum.value() / dram_cycles;
    }

    long page_allocator(long addr, int coreid) {
        long virtual_page_number = addr >> 12;

        switch(int(translation)) {
            case int(Translation::None): {
              auto target = make_pair(coreid, virtual_page_number);
              if(page_translation.find(target) == page_translation.end()) {
                memory_footprint += 1<<12;
                page_translation[target] = virtual_page_number;
              }
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
    long lrand(void) {
        if(sizeof(int) < sizeof(long)) {
            return static_cast<long>(rand()) << (sizeof(int) * 8) | rand();
        }

        return rand();
    }
};

} /*namespace ramulator*/

#endif /*__MEMORY_H*/
