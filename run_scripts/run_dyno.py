from subprocess import call
import sys

single_threaded =[
                "bfs_coAuthorsDBLP.graph.el",
                "bfs_ny_sandy.graph.el",
                "bfs_cond-mat-2003.graph.el",
                ]
workloads=[[
            "bfs_coAuthorsDBLP.graph.el",
            "bfs_ny_sandy.graph.el",
            "bfs_cond-mat-2003.graph.el",
            ]]

ramulator_bin = "/home/tianshi/Workspace/ramulator/ramulator"

if not len(sys.argv) == 7:
  print "python run_mediabench.py trace_dir output_dir config_dir workloadid DRAM [multicore|single-threaded]"
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
