from subprocess import call
import sys

single_threaded = ["mcf", "libquantum", "milc", "GemsFDTD", "bwaves", "soplex", "omnetpp", "gcc", "cactusADM", "zeusmp", "gobmk", "astar", "namd", "sphinx3", "sjeng", "bzip2", "hmmer", "perlbench", "h264ref", "calculix", "povray", "gamess"]
workloads = [["milc", "GemsFDTD", "mcf", "libquantum"], ["bwaves", "omnetpp", "mcf", "libquantum"], ["libquantum", "bwaves", "soplex", "GemsFDTD"], ["soplex", "mcf", "omnetpp", "milc"], ["milc", "mcf", "GemsFDTD", "h264ref"], ["soplex", "omnetpp", "milc", "namd"], ["libquantum", "omnetpp", "bwaves", "povray"], ["libquantum", "mcf", "milc", "zeusmp"], ["omnetpp", "GemsFDTD", "cactusADM", "hmmer"], ["GemsFDTD", "mcf", "gamess", "zeusmp"], ["milc", "mcf", "bzip2", "h264ref"], ["bwaves", "soplex", "gamess", "namd"], ["omnetpp", "sjeng", "namd", "gcc"], ["GemsFDTD", "hmmer", "zeusmp", "astar"], ["GemsFDTD", "povray", "sphinx3", "calculix"], ["soplex", "zeusmp", "sphinx3", "gcc"], ["povray", "astar", "gobmk", "perlbench"], ["povray", "bzip2", "sphinx3", "cactusADM"], ["astar", "sjeng", "gcc", "cactusADM"], ["calculix", "namd", "perlbench", "gamess"]]

ramulator_bin = "/home/tianshi/Workspace/ramulator/ramulator"

if not len(sys.argv) == 7:
  print "python run_spec.py trace_dir output_dir config_dir workloadid DRAM [multicore|single-threaded]"
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

  print " ".join([ramulator_bin, config, "--mode=cpu", "--stats", output] + traces)
  call([ramulator_bin, config, "--mode=cpu", "--stats", output] + traces)
elif option == "single-threaded":
  trace = trace_dir + "/" + single_threaded[workload_id] + ".trace"
  print " ".join([ramulator_bin, config, "--mode=cpu", "--stats", output, trace])
  call([ramulator_bin, config, "--mode=cpu", "--stats", output, trace])
