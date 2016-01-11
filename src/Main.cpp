#include "Processor.h"
#include "Config.h"
#include "Controller.h"
#include "SpeedyController.h"
#include "Memory.h"
#include "DRAM.h"
#include "Statistics.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

static int gcd(int u, int v) {
  if (v > u) {
    swap(u,v);
  }
  while (v != 0) {
    int r = u % v;
    u = v;
    v = r;
  }
  return u;
}

template<typename T>
void run_dramtrace(const Config& configs, Memory<T, Controller>& memory, const char* tracename) {

    /* initialize DRAM trace */
    Trace trace(tracename);

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
            end = !trace.get_dramtrace_request(addr, type);
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
        Stats::curTick++; // memory clock, global, for Statistics
    }
    // This a workaround for statistics set only initially lost in the end
    memory.finish();
    Stats::statlist.printall();

}

template <typename T>
void run_cputrace(const Config& configs, Memory<T, Controller>& memory, const std::vector<const char *>& files)
{
    // time unit is ps
    int cpu_tick = configs.get_cpu_tick();
    int mem_tick = memory.clk_ns() * 1000;
    int tick_gcd = gcd(cpu_tick, mem_tick);
    printf("tick_gcd: %d\n", tick_gcd);
    cpu_tick /= tick_gcd;
    printf("cpu_tick: %d\n", cpu_tick);
    mem_tick /= tick_gcd;
    printf("mem_tick: %d\n", mem_tick);
    long next_cpu_tick = cpu_tick - 1;
    long next_mem_tick = mem_tick - 1;

    auto send = bind(&Memory<T, Controller>::send, &memory, placeholders::_1);
    Processor proc(configs, files, send, memory);
    for (long i = 0; ; i++) {
      if (i == next_cpu_tick) {
        next_cpu_tick += cpu_tick;
        proc.tick();
        Stats::curTick++; // processor clock, global, for Statistics
        if (configs.calc_weighted_speedup()) {
          if (proc.has_reached_limit()) {
            break;
          }
        } else {
          if (configs.is_early_exit()) {
            if (proc.finished())
                break;
          } else {
            if (proc.finished() && (memory.pending_requests() == 0))
                break;
          }
        }
//         printf("cpu tick: %ld\n", i/1000);
      }
      if (i == next_mem_tick) {
        next_mem_tick += mem_tick;
        memory.tick();
//         printf("mem tick: %ld\n", i/1000);
      }
    }
    // This a workaround for statistics set only initially lost in the end
    memory.finish();
    Stats::statlist.printall();
}

template<typename T>
void start_run(const Config& configs, T* spec, const vector<const char*>& files) {
  // initiate controller and memory
  int C = configs.get_channels(), R = configs.get_ranks();
  // Check and Set channel, rank number
  spec->set_channel_number(C);
  spec->set_rank_number(R);
  std::vector<Controller<T>*> ctrls;
  for (int c = 0 ; c < C ; c++) {
    DRAM<T>* channel = new DRAM<T>(spec, T::Level::Channel);
    channel->id = c;
    channel->regStats("");
    Controller<T>* ctrl = new Controller<T>(configs, channel);
    ctrls.push_back(ctrl);
  }
  Memory<T, Controller> memory(configs, ctrls);

  assert(files.size() != 0);
  if (configs["trace_type"] == "CPU") {
    run_cputrace(configs, memory, files);
  } else if (configs["trace_type"] == "DRAM") {
    run_dramtrace(configs, memory, files[0]);
  }
}

template<>
void start_run<HMC>(const Config& configs, T* spec, const vector<const char*>& files) {
  int V = configs.get_vaults(), S = configs.get_stacks();
  int total_vault_number = V * S;
  std::vector<Controller<HMC>*> vault_ctrls;
  for (int c = 0 ; c < total_vault_number ; ++c) {
    DRAM<HMC>* vault = new DRAM<HMC>(spec, HMC::Level::Vault);
    vault->id = c;
    vault->regStats("");
    Controller<HMC>* ctrl = new Controller<HMC>(configs, vault);
    ctrls.push_back(ctrl);
  }
  Memory<HMC, Controller> memory(configs, vault_ctrls);

  assert(files.size() != 0);
  if (configs["trace_type"] == "CPU") {
    run_cputrace(configs, memory, files);
  } else if (configs["trace_type"] == "DRAM") {
    run_dramtrace(configs, memory, files[0]);
  }
}

int main(int argc, const char *argv[])
{
    if (argc < 2) {
        printf("Usage: %s <configs-file> --mode=cpu,dram [--stats <filename>] <trace-filename1> <trace-filename2>\n"
            "Example: %s ramulator-configs.cfg --mode=cpu cpu.trace cpu.trace\n", argv[0], argv[0]);
        return 0;
    }

    Config configs(argv[1]);

    const std::string& standard = configs["standard"];
    assert(standard != "" || "DRAM standard should be specified.");

    const char *trace_type = strstr(argv[2], "=");
    trace_type++;
    if (strcmp(trace_type, "cpu") == 0) {
      configs.add("trace_type", "CPU");
    } else if (strcmp(trace_type, "dram") == 0) {
      configs.add("trace_type", "DRAM");
    } else {
      printf("invalid trace type: %s\n", trace_type);
      assert(false);
    }

    int trace_start = 3;
    string stats_out;
    if (strcmp(argv[3], "--stats") == 0) {
      Stats::statlist.output(argv[4]);
      stats_out = argv[4];
      trace_start = 5;
    } else {
      Stats::statlist.output(standard+".stats");
      stats_out = standard + string(".stats");
    }
    std::vector<const char*> files(&argv[trace_start], &argv[argc]);
    configs.set_core_num(argc - trace_start);

    if (standard == "DDR3") {
      DDR3* ddr3 = new DDR3(configs["org"], configs["speed"]);
      start_run(configs, ddr3, files);
    } else if (standard == "DDR4") {
      DDR4* ddr4 = new DDR4(configs["org"], configs["speed"]);
      start_run(configs, ddr4, files);
    } else if (standard == "SALP-MASA") {
      SALP* salp8 = new SALP(configs["org"], configs["speed"], "SALP-MASA", configs.get_subarrays());
      start_run(configs, salp8, files);
    } else if (standard == "LPDDR3") {
      LPDDR3* lpddr3 = new LPDDR3(configs["org"], configs["speed"]);
      start_run(configs, lpddr3, files);
    } else if (standard == "LPDDR4") {
      // total cap: 2GB, 1/2 of others
      LPDDR4* lpddr4 = new LPDDR4(configs["org"], configs["speed"]);
      start_run(configs, lpddr4, files);
    } else if (standard == "GDDR5") {
      GDDR5* gddr5 = new GDDR5(configs["org"], configs["speed"]);
      start_run(configs, gddr5, files);
    } else if (standard == "HBM") {
      HBM* hbm = new HBM(configs["org"], configs["speed"]);
      start_run(configs, hbm, files);
    } else if (standard == "WideIO") {
      // total cap: 1GB, 1/4 of others
      WideIO* wio = new WideIO(configs["org"], configs["speed"]);
      start_run(configs, wio, files);
    } else if (standard == "WideIO2") {
      // total cap: 2GB, 1/2 of others
      WideIO2* wio2 = new WideIO2(configs["org"], configs["speed"], configs.get_channels());
      wio2->channel_width *= 2;
      start_run(configs, wio2, files);
    }
    // Various refresh mechanisms
      else if (standard == "DSARP") {
      DSARP* dsddr3_dsarp = new DSARP(configs["org"], configs["speed"], DSARP::Type::DSARP, configs.get_subarrays());
      start_run(configs, dsddr3_dsarp, files);
    } else if (standard == "ALDRAM") {
      ALDRAM* aldram = new ALDRAM(configs["org"], configs["speed"]);
      start_run(configs, aldram, files);
    } else if (standard == "TLDRAM") {
      TLDRAM* tldram = new TLDRAM(configs["org"], configs["speed"], configs.get_subarrays());
      start_run(configs, tldram, files);
    } else if (standard == "HMC") {
      HMC* hmc = new HMC(configs["org"], configs["speed"], configs["maxblock"],
          configs["link_width"], configs["lane_speed"],
          configs.get_value("source_mode_host_links"),
          configs.get_value("payload_flits"));
      start_run(configs, hmc, files);
    }

    printf("Simulation done. Statistics written to %s\n", stats_out.c_str());

    return 0;
}
