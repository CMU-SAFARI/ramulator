from subprocess import call
import sys

single_threaded =[
                  "iozone_64m_r4k_0",
                  "iozone_64m_r4k_10",
                  "iozone_64m_r4k_11",
                  "iozone_64m_r4k_12",
                  "iozone_64m_r4k_1",
                  "iozone_64m_r4k_2",
                  "iozone_64m_r4k_3",
                  "iozone_64m_r4k_4",
                  "iozone_64m_r4k_5",
                  "iozone_64m_r4k_6",
                  "iozone_64m_r4k_7",
                  "iozone_64m_r4k_8",
                  "iozone_64m_r4k_9",
                  "iozone_256m_r4k_0",
                  "iozone_256m_r4k_10",
                  "iozone_256m_r4k_11",
                  "iozone_256m_r4k_12",
                  "iozone_256m_r4k_1",
                  "iozone_256m_r4k_2",
                  "iozone_256m_r4k_3",
                  "iozone_256m_r4k_4",
                  "iozone_256m_r4k_5",
                  "iozone_256m_r4k_6",
                  "iozone_256m_r4k_7",
                  "iozone_256m_r4k_8",
                  "iozone_256m_r4k_9",
                  "iozone_256m_r1m_0",
                  "iozone_256m_r1m_10",
                  "iozone_256m_r1m_11",
                  "iozone_256m_r1m_12",
                  "iozone_256m_r1m_1",
                  "iozone_256m_r1m_2",
                  "iozone_256m_r1m_3",
                  "iozone_256m_r1m_4",
                  "iozone_256m_r1m_5",
                  "iozone_256m_r1m_6",
                  "iozone_256m_r1m_7",
                  "iozone_256m_r1m_8",
                  "iozone_256m_r1m_9",
                  "netperf_tcpstream_v4",
                  "netperf_udpstream_v4",
                  "netperf_tcprr_v4",
                  "netperf_udprr_v4"
                ]

ramulator_bin = "/home/tianshi/tianshi-Workspace/ramulator/ramulator"

if not len(sys.argv) == 7:
  print "python %s trace_dir output_dir config_dir workloadid DRAM [multicore|single-threaded]" % (sys.argv[0])
  sys.exit(0)

option = sys.argv[6]
DRAM = sys.argv[5]
workload_id = int(sys.argv[4])
config_dir = sys.argv[3]
output_dir = sys.argv[2]
trace_dir = sys.argv[1]

if option == "multicore":
  output_dir += "/workload" + str(workload_id)
  print output_dir
else:
  output_dir += "/" + single_threaded[workload_id]
  print output_dir

call(["mkdir", "-p", output_dir])

output = output_dir + "/" + DRAM + ".stats"
config = config_dir + "/" + DRAM + "-config.cfg"

if option == "multicore":
  traces = [trace_dir + "/" + t + ".trace" for t in workloads[workload_id]]

  cmds = [ramulator_bin, "--config", config, "--mode", "cpu", "--stats", output, "--translation", "None", "--expected-limit-insts", str(expected_limit_insts), "--trace"] + traces
  print " ".join(cmds)
  call(cmds)
elif option == "single-threaded":
  trace = trace_dir + "/" + single_threaded[workload_id] + ".trace"
  cmds = [ramulator_bin, "--config", config, "--mode", "cpu", "--stats", output, "--translation", "None", "--trace", trace]
  print " ".join(cmds)
  call(cmds)
