from subprocess import call
import sys
from Hadoop import *
from SPEC2006 import *
from Mediabench2 import *
from Redis import *

ramulator_bin = "/home/tianshi/tianshi-Workspace/ramulator/ramulator"

configs = {"spec2006": SPEC2006(), "mediabench2": Mediabench2(), "redis": Redis(), "hadoop": Hadoop()}

if not len(sys.argv) >= 8:
  print "python %s workload trace_dir output_dir config_dir workloadid DRAM [multicore|single-threaded]" % sys.argv[0]
  sys.exit(0)

# FIXME specify this argument by commandline argument
expected_limit_insts = 1886863202

if len(sys.argv) > 8:
  extra_arg = sys.argv[8]
else:
  extra_arg = ""
option = sys.argv[7]
DRAM = sys.argv[6]
workload_id = int(sys.argv[5])
config_dir = sys.argv[4]
output_dir = sys.argv[3]
trace_dir = sys.argv[2]
print "select workload: %s" % (sys.argv[1])
if sys.argv[1] in configs:
  config = configs[sys.argv[1]]
else:
  assert(KeyError)

if option == "multicore":
  output_dir += "/workload" + str(workload_id)
  print output_dir
else:
  output_dir += "/" + config.single_threaded[workload_id]
  print output_dir

call(["mkdir", "-p", output_dir])

output_path = output_dir + "/" + DRAM + ".stats"
config_path = config_dir + "/" + DRAM + "-config.cfg"

if option == "multicore":
  traces = [trace_dir + "/" + t + ".trace" for t in config.workloads[workload_id]]

  cmds = [ramulator_bin, "--config", config_path, "--mode", "cpu", "--stats", output_path, "--expected-limit-insts", str(expected_limit_insts), "--trace"] + traces + extra_arg.split()
  print " ".join(cmds)
  call(cmds)
elif option == "single-threaded":
  trace = trace_dir + "/" + config.single_threaded[workload_id] + ".trace"
  cmds = [ramulator_bin, "--config", config_path, "--mode", "cpu", "--stats", output_path, "--trace", trace] + extra_arg.split()
  print " ".join(cmds)
  call(cmds)
