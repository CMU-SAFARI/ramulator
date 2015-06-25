#include "Processor.h"
#include "Controller.h"
#include "SpeedyController.h"
#include "Memory.h"
#include "DRAM.h"
#include <cstdio>
#include <cstdlib>
#include <stdlib.h>
#include <functional>
#include <map>

/* Standards */
#include "Gem5Wrapper.h"
#include "DDR3.h"
#include "DDR4.h"
#include "DSARP.h"
#include "GDDR5.h"
#include "LPDDR3.h"
#include "LPDDR4.h"
#include "WideIO.h"
#include "WideIO2.h"
#include "HBM.h"
#include "SALP.h"
#include "ALDRAM.h"
#include "TLDRAM.h"

using namespace std;
using namespace ramulator;


#ifdef RAMULATOR_DRAMTRACE

int main (int argc, char** argv)
{
    int C = 1, R = 1;  // channels, ranks
    if (argc < 2) {
        printf("Usage: %s <dram-trace-file> [<channels> <ranks>]\n Example: %s dram.trace 1 1", argv[0], argv[0]);
        return 0;
    }
    if (argc >= 3) C = strtol(argv[2], NULL, 10);
    if (argc >= 4) R = strtol(argv[3], NULL, 10);

    /* initialize DRAM system */
    DDR3* ddr3 = new DDR3(DDR3::Org::DDR3_2Gb_x8, DDR3::Speed::DDR3_1600K);
    vector<SpeedyController<DDR3>*> ctrls;
    for (int c = 0; c < C; c++) {
        DRAM<DDR3>* channel = new DRAM<DDR3>(ddr3, DDR3::Level::Channel);
        channel->id = c;
        for (int r = 0; r < R; r++)
            channel->insert(new DRAM<DDR3>(ddr3, DDR3::Level::Rank));

        SpeedyController<DDR3>* ctrl = new SpeedyController<DDR3>(channel);
        ctrls.push_back(ctrl);
    }
    Memory<DDR3, SpeedyController> memory(ctrls);

    /* initialize DRAM trace */
    Trace trace(argv[1]);

    /* run simulation */
    bool stall = false, end = false;
    int reads = 0, writes = 0, clks = 0;
    long addr = 0;
    Request::Type type = Request::Type::READ;
    map<int, int> latencies;
    auto read_complete = [&latencies](Request& r){latencies[r.depart - r.arrive]++;};

    Request req(addr, type, read_complete);

    while (!end || memory.pending_requests()){
        if (!end && !stall){
            end = !trace.get_request(addr, type);
        }

        if (!end){
            req.addr = addr;
            req.type = type;
            stall = !memory.send(req);
            if (!stall){
                if (type == Request::Type::READ) reads++;
                else if (type == Request::Type::WRITE) writes++;
            }
        }
        memory.tick();
        clks ++;
    }

    /* report statistics */

    double tCK = memory.clk_ns(), t = tCK * clks;
    double rbw = 64.0 * reads / 1024 / 1024 / 1024 / (t / 1e9);
    double wbw = 64.0 * writes / 1024 / 1024 / 1024 / (t / 1e9);

    printf("Simulation done %d clocks [%.3lfns], %d reads [%.3lf GB/s], %d writes [%.3lf GB/s]\n",
        clks, t, reads, rbw, writes, wbw);

    /* histogram of read latencies */
    // long total_latency = 0;
    // for (auto& kv : latencies){
    //     printf("[%4d]: %8d\n", kv.first, kv.second);
    //     total_latency += kv.first * kv.second;
    // }
    // printf("Avg latency: %.3lf ns\n", tCK * total_latency / reads);

    return 0;
}

#endif



#ifdef RAMULATOR_CPUTRACE

template <typename T>
double run_simulation(T *spec, const char *file, int chan, int rank, int cpu_tick, int mem_tick)
{
    vector<Controller<T>*> ctrls;
    for (int c = 0; c < chan; c++){
        DRAM<T> *dram = new DRAM<T>(spec, T::Level::Channel);
        dram->id = c;
        for (int r = 0; r < rank; r++)
            dram->insert(new DRAM<T>(spec, T::Level::Rank));
        Controller<T> *ctrl = new Controller<T>(dram);
        ctrls.push_back(ctrl);
    }
    Memory<T, Controller> memory(ctrls);
    auto send = bind(&Memory<T, Controller>::send, &memory, placeholders::_1);
    Processor proc(file, send);
    for (long i = 0; ; i++) {
        // if (i % 100000000 == 0) printf("%ld clocks\n", i);
        proc.tick();
        if (i % cpu_tick == (cpu_tick - 1))
            for (int j = 0; j < mem_tick; j++)
                memory.tick();
        if (proc.finished() && memory.pending_requests() == 0)
            break;
    }
    return proc.calc_ipc();
}

int main(int argc, const char *argv[])
{
    if (argc < 2){
        printf("Usage: %s <cpu-trace-file>\n Example: %s cpu.trace", argv[0], argv[0]);
        return 0;
    }

    double baseIPC = 4, IPC = 0;
    DDR3* ddr3 = new DDR3(DDR3::Org::DDR3_8Gb_x8, DDR3::Speed::DDR3_1333H);
    baseIPC = run_simulation(ddr3, argv[1], 1, 1, 4, 1);
    printf("%10s: %.5lf\n", "DDR3", 1.0);

    DDR4* ddr4 = new DDR4(DDR4::Org::DDR4_4Gb_x8, DDR4::Speed::DDR4_2400R);
    IPC = run_simulation(ddr4, argv[1], 1, 1, 8, 3);
    printf("%10s: %.5lf\n", "DDR4", IPC / baseIPC);

    SALP* salp8 = new SALP(SALP::Org::SALP_4Gb_x8, SALP::Speed::SALP_1600K, SALP::Type::MASA, 8);
    IPC = run_simulation(salp8, argv[1], 1, 1, 4, 1);
    printf("%10s: %.5lf\n", "SALP", IPC / baseIPC);

    LPDDR3* lpddr3 = new LPDDR3(LPDDR3::Org::LPDDR3_8Gb_x16, LPDDR3::Speed::LPDDR3_1600);
    IPC = run_simulation(lpddr3, argv[1], 1, 1, 4, 1);
    printf("%10s: %.5lf\n", "LPDDR3", IPC / baseIPC);

    // total cap: 2GB, 1/2 of others
    LPDDR4* lpddr4 = new LPDDR4(LPDDR4::Org::LPDDR4_8Gb_x16, LPDDR4::Speed::LPDDR4_2400);
    IPC = run_simulation(lpddr4, argv[1], 2, 1, 8, 3);
    printf("%10s: %.5lf\n", "LPDDR4", IPC / baseIPC);

    GDDR5* gddr5 = new GDDR5(GDDR5::Org::GDDR5_8Gb_x16, GDDR5::Speed::GDDR5_6000);
    IPC = run_simulation(gddr5, argv[1], 1, 1, 2, 1); //6400 overclock
    printf("%10s: %.5lf\n", "GDDR5", IPC / baseIPC);

    HBM* hbm = new HBM(HBM::Org::HBM_4Gb, HBM::Speed::HBM_1Gbps);
    IPC = run_simulation(hbm, argv[1], 8, 1, 32, 5);
    printf("%10s: %.5lf\n", "HBM", IPC / baseIPC);

    // total cap: 1GB, 1/4 of others
    WideIO* wio = new WideIO(WideIO::Org::WideIO_8Gb, WideIO::Speed::WideIO_266);
    IPC = run_simulation(wio, argv[1], 4, 1, 12, 1);
    printf("%10s: %.5lf\n", "WideIO", IPC / baseIPC);

    // total cap: 2GB, 1/2 of others
    WideIO2* wio2 = new WideIO2(WideIO2::Org::WideIO2_8Gb, WideIO2::Speed::WideIO2_1066);
    wio2->channel_width *= 2;
    IPC = run_simulation(wio2, argv[1], 8, 1, 6, 1);
    printf("%10s: %.5lf\n", "WideIO2", IPC / baseIPC);

    // Various refresh mechanisms
    DSARP* dsddr3_dsarp = new DSARP(DSARP::Org::DSARP_8Gb_x8,
        DSARP::Speed::DSARP_1333, DSARP::Type::DSARP, 8);
    IPC = run_simulation(dsddr3_dsarp, argv[1], 1, 1, 4, 1);
    printf("%10s: %.5lf\n", "DSARP", IPC / baseIPC);

    ALDRAM* aldram = new ALDRAM(ALDRAM::Org::ALDRAM_4Gb_x8, ALDRAM::Speed::ALDRAM_1600K);
    IPC = run_simulation(aldram, argv[1], 1, 1, 4, 1);
    printf("%10s: %.5lf\n", "ALDRAM", IPC / baseIPC);

    TLDRAM* tldram = new TLDRAM(TLDRAM::Org::TLDRAM_4Gb_x8, TLDRAM::Speed::TLDRAM_1600K, 16);
    IPC = run_simulation(tldram, argv[1], 1, 1, 4, 1);
    printf("%10s: %.5lf\n", "TLDRAM", IPC / baseIPC);

    return 0;
}
#endif /*test spec2006*/
