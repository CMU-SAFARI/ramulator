from workload_stats import *
from subprocess import call
import argparse

class Config(object):
  def __init__(self):
    self.workload_dir = None
    self.workload = None
    self.DRAM_list = None
    self.ref_DRAM = None
    self.single_threaded_workload_list = None

parser = argparse.ArgumentParser(description="Process ramulator statistics.")

parser.add_argument('--workload-dir', required=True, help="input workload dir")
parser.add_argument('--workload-list', required=True, nargs="+", help="input a list of workloads")
parser.add_argument('--dram-list', required=True, nargs="+", help="input a list of DRAM")
parser.add_argument('--ref-dram', required=True, help="input the reference DRAM that is used to be normalized to and reorder")
parser.add_argument('--single-threaded-workload-lists', nargs="+", help="input a list of the corresponding single-threaded workload list that is used for weighted speedup calculation (required only when the target workload is multiprogram)")
parser.add_argument('--output-dir', required=True, help="output directory")

args = vars(parser.parse_args())

configs = Config()

configs.workload_dir = args["workload_dir"]
configs.DRAM_list = args["dram_list"]
configs.ref_DRAM = args["ref_dram"]
single_threaded_workload_lists = args["single_threaded_workload_lists"]
workload_list = args["workload_list"]

class output_gen:
  def __init__(self, output_dir):
    self.out_f = dict()
    self.output_dir = output_dir
    call(["mkdir", "-p", output_dir])

  def __getitem__(self, item):
    if not item in self.out_f:
      self.out_f[item] = open(self.output_dir + "/" + item + ".csv", "w")
      print "%s statistics are written to %s" % (item, self.output_dir + "/" + item + ".csv")
    return self.out_f[item]

  def write_to_all(self, item_list, content):
    for item in item_list:
      self[item].write(content)

out_f = output_gen(args["output_dir"])
R_dataframe_out_f = output_gen(args["output_dir"] + "_R_dataframe")

workload_stats_list = []

if single_threaded_workload_lists is None: # single-threaded workload
  for workload in workload_list:
    configs.workload = workload
    w = workload_stats(configs)
    w.normalize("ipc")
    workload_stats_list.append(w)
else: # multi-program workload
  if (not len(single_threaded_workload_lists) == len(workload_list)):
    print "workload_list", workload_list
    print "single_threaded_workload_lists", single_threaded_workload_lists
    assert(len(single_threaded_workload_lists) == len(workload_list))
  for single_threaded_workload_list, workload in zip(single_threaded_workload_lists, workload_list):
    configs.single_threaded_workload_list = single_threaded_workload_list.split(",")
    configs.workload = workload
    w = workload_stats(configs)
    w.normalize("weighted_speedup")
    workload_stats_list.append(w)

if single_threaded_workload_lists is None: # single-threaded workload
  workload_stats_list.sort(key = lambda x : x.ref_stats.mem_stats["MPKI"], reverse=True)
else:
  workload_stats_list.sort(key = lambda x : sum([s["MPKI"] for s in x.ref_stats.ref_mem_stats]), reverse=True)

# first write header for 1D workload, which is fixed for all statistics
R_dataframe_out_f.write_to_all(["MPKI", "ipc", "weighted_speedup", "locality", "bandwidth_utilization", "average_latency_ns", "normalized_ipc", "normalized_weighted_speedup"], ",".join(["statistics", "value", "DRAM", "workload", "workload_feature", "workload_and_feature", "ref_MPKI"]) + "\n")

for w in workload_stats_list:
# 2D format
  w.append_2D_to(["MPKI"], out_f["MPKI"])
  if single_threaded_workload_lists is None:
    w.append_2D_to(["ipc"], out_f["ipc"])
    w.append_2D_to(["normalized_ipc"], out_f["normalized_ipc"])

  else:
    w.append_2D_to(["weighted_speedup"], out_f["weighted_speedup"])
    w.append_2D_to(["normalized_weighted_speedup"], out_f["normalized_weighted_speedup"])
  w.append_2D_to(["row_hit_rate", "row_miss_rate", "row_conflict_rate"], out_f["locality"])
  w.append_2D_to(["bandwidth_utilization"], out_f["bandwidth_utilization"])
  w.append_2D_to(["request_packet_latency_ns_avg", "queueing_latency_ns_avg", "DRAM_latency_ns_avg", "response_packet_latency_ns_avg"], out_f["average_latency_ns"])

# 1D format (one row for one value, for R dataframe)

  w.append_1D_to(["MPKI"], R_dataframe_out_f["MPKI"])
  if single_threaded_workload_lists is None:
    w.append_1D_to(["ipc"], R_dataframe_out_f["ipc"])
    w.append_1D_to(["normalized_ipc"], R_dataframe_out_f["normalized_ipc"])

  else:
    w.append_1D_to(["weighted_speedup"], R_dataframe_out_f["weighted_speedup"])
    w.append_1D_to(["normalized_weighted_speedup"], R_dataframe_out_f["normalized_weighted_speedup"])
  w.append_1D_to(["row_hit_rate", "row_miss_rate", "row_conflict_rate"], R_dataframe_out_f["locality"])
  w.append_1D_to(["bandwidth_utilization"], R_dataframe_out_f["bandwidth_utilization"])
  w.append_1D_to(["request_packet_latency_ns_avg", "queueing_latency_ns_avg", "DRAM_latency_ns_avg", "response_packet_latency_ns_avg"], R_dataframe_out_f["average_latency_ns"])
