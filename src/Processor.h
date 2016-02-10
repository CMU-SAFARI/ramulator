#ifndef __PROCESSOR_H
#define __PROCESSOR_H

#include "Cache.h"
#include "Config.h"
#include "Memory.h"
#include "Request.h"
#include "Statistics.h"
#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <ctype.h>
#include <functional>

namespace ramulator 
{

class Trace {
public:
    Trace(const char* trace_fname);
    // trace file format 1:
    // [# of bubbles(non-mem instructions)] [read address(dec or hex)] <optional: write address(evicted cacheline)>
    bool get_unfiltered_request(long& bubble_cnt, long& req_addr, Request::Type& req_type);
    bool get_filtered_request(long& bubble_cnt, long& req_addr, Request::Type& req_type);
    // trace file format 2:
    // [address(hex)] [R/W]
    bool get_dramtrace_request(long& req_addr, Request::Type& req_type);

private:
    std::ifstream file;
    std::string trace_name;
};


class Window {
public:
    int ipc = 4;
    int depth = 128;

    Window() : ready_list(depth), addr_list(depth, -1) {}
    bool is_full();
    bool is_empty();
    void insert(bool ready, long addr);
    long retire();
    void set_ready(long addr, int mask);

private:
    int load = 0;
    int head = 0;
    int tail = 0;
    std::vector<bool> ready_list;
    std::vector<long> addr_list;
};


class Core {
public:
    long clk = 0;
    long retired = 0;
    int id = 0;
    function<bool(Request)> send;

    Core(const Config& configs, int coreid,
        const char* trace_fname,
        function<bool(Request)> send_next, Cache* llc,
        std::shared_ptr<CacheSystem> cachesys, MemoryBase& memory);
    void tick();
    void receive(Request& req);
    double calc_ipc();
    bool finished();
    bool has_reached_limit();
    function<void(Request&)> callback;

    bool no_core_caches = true;
    bool no_shared_cache = true;
    int l1_size = 1 << 15;
    int l1_assoc = 1 << 3;
    int l1_blocksz = 1 << 6;
    int l1_mshr_num = 16;

    int l2_size = 1 << 18;
    int l2_assoc = 1 << 3;
    int l2_blocksz = 1 << 6;
    int l2_mshr_num = 16;
    std::vector<std::shared_ptr<Cache>> caches;
    Cache* llc;

    ScalarStat record_cycs;
    ScalarStat record_insts;
    long expected_limit_insts;
    // This is set true iff expected number of instructions has been executed or all instructions are executed.
    bool reached_limit = false;;

private:
    Trace trace;
    Window window;

    long bubble_cnt;
    long req_addr = -1;
    Request::Type req_type;
    bool more_reqs;
    long last = 0;

    ScalarStat memory_access_cycles;
    ScalarStat cpu_inst;
    MemoryBase& memory;
};

class Processor {
public:
    Processor(const Config& configs, vector<const char*> trace_list,
        function<bool(Request)> send, MemoryBase& memory);
    void tick();
    void receive(Request& req);
    bool finished();
    bool has_reached_limit();

    std::vector<std::unique_ptr<Core>> cores;
    std::vector<double> ipcs;
    double ipc = 0;

    // When early_exit is true, the simulation exits when the earliest trace finishes.
    bool early_exit;

    bool no_core_caches = true;
    bool no_shared_cache = true;

    int l3_size = 1 << 23;
    int l3_assoc = 1 << 3;
    int l3_blocksz = 1 << 6;
    int mshr_per_bank = 16;

    std::shared_ptr<CacheSystem> cachesys;
    Cache llc;

    ScalarStat cpu_cycles;
};

}
#endif /* __PROCESSOR_H */
