from subprocess import call
import sys

single_threaded =[
                "trace1-inflight-5",
                "trace1-inflight-10",
                "trace1-inflight-20",
                "trace1-inflight-50",
                "trace4-inflight-5",
                "trace4-inflight-10",
                "trace4-inflight-20",
                "trace4-inflight-50",
                ]

DRAM_list = [
  "DDR3-2133L",
  "DDR4-2400R",
  "LPDDR3-2133",
  "LPDDR4-3200",
  "GDDR5-7000",
  "HBM-1000",
  "WideIO-266",
  "WideIO2-1067"
  "HMC",
  "HMC-RoBaCoVa"
  "DDR3-800D",
  "DDR3-1066E",
  "DDR3-1333G",
  "DDR3-1600H",
  "DDR3-1866K",
  "DDR3-2133L-bank128",
  "DDR3-2133L-bank16",
  "DDR3-2133L-bank256",
  "DDR3-2133L-bank32",
  "DDR3-2133L-bank64",
  "GDDR5-7000-bank128",
  "GDDR5-7000-bank256",
  "GDDR5-7000-bank32",
  "GDDR5-7000-bank512",
  "GDDR5-7000-bank64",
  "HBM-1000-bank128",
  "HBM-1000-bank256",
  "HBM-1000-bank32",
  "HBM-1000-bank512",
  "HBM-1000-bank64",
  "HMC-bank128",
  "HMC-bank16",
  "HMC-bank256",
  "HMC-bank32",
  "HMC-bank64",
  "DDR3-2133L-ideal-lat-BW",
  "GDDR5-7000-ideal-lat-BW",
  "HBM-1000-ideal-lat-BW",
  "HMC-ideal-lat-BW",
  "DDR3-2133L-noDRAMlat",
  "GDDR5-7000-noDRAMlat",
  "HBM-1000-noDRAMlat",
  "HMC-noDRAMlat",
  "DDR3-2133L-unlimitBW",
  "GDDR5-7000-unlimitBW",
  "HBM-1000-unlimitBW",
  "HMC-unlimitBW",
  "HMC-va64",
  "HMC-va128",
  "HMC-va256",
  "HMC-va512",
  "HMC-va1024"
]

ramulator_bin = "/home/tianshi/tianshi-Workspace/ramulator/ramulator"

if not len(sys.argv) == 4:
  print "python %s trace_dir output_dir config_dir" % (sys.argv[0])
  sys.exit(0)

config_dir = sys.argv[3]
output_dir = sys.argv[2]
trace_dir = sys.argv[1]

for workload_id in range(len(single_threaded)):
  output_dir = sys.argv[2] + "/" + single_threaded[workload_id]
  print output_dir
  call(["mkdir", "-p", output_dir])

  l = single_threaded[workload_id].split("-")
  trace_name = l[0]
  trace = trace_dir + "/" + trace_name + ".trace"
  inflight_limit = l[2]

  for DRAM in DRAM_list:
    output = output_dir + "/" + DRAM + ".stats"
    config = config_dir + "/" + DRAM + "-config.cfg"
    cmds = [ramulator_bin, "--mode", "dram", "--config", config, "--cache", "no", "--trace", trace, "--inflight-limit", inflight_limit, "--translation", "None", "--stats", output]

    print(" ".join(cmds))
    call(cmds)

