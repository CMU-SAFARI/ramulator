import sys
import os.path

### This python script is used for extract structural data from gem5 stats.txt

### Here I list all the statistics I concerns, to be developed ...
# memory_bandwidth
# IPC
# CPI
# cache miss rate
# incoming requests
# row buffer hit/miss
# memory latency sum (average memory latency)
# ACT count
# non auto PRE count
# time wait to issue
# memory controller queue length
# refresh cycle
# active cycle
# busy cycle
# simultaneouly serving requests

stat_map = dict()
multidim_stats = []

### We ignore most gem5 statistics, except for:
# sim_insts
# system.cpu.dcache.overall_accesses::switch_cpus.data
# system.mem_ctrls.bw_total::total
# system.mem_ctrls.bw_write::total
# system.mem_ctrls.bw_read::total
# system.switch_cpus.cpi
# system.switch_cpus.cpi_total
# system.cpu.dcache.overall_miss_rate::total
# system.cpu.l2cache.overall_miss_rate::total
# system.l3.overall_miss_rate::total
#
# stat_map["sim_insts"] = "sim_insts"
# stat_map["system.cpu.dcache.overall_accesses::total"] = "l1_access"
# stat_map["system.mem_ctrls.bw_total::total"] = "memory_bandwidth"
# stat_map["system.mem_ctrls.bw_write::total"] = "write_bandwidth"
# stat_map["system.mem_ctrls.bw_read::total"] = "read_bandwidth"
# stat_map["system.switch_cpus.ipc"] = "ipc"
# stat_map["system.switch_cpus.cpi"] = "cpi"
# stat_map["system.switch_cpus.ipc_total"] = "ipc_total"
# stat_map["system.switch_cpus.cpi_total"] = "cpi_total"
# stat_map["system.cpu.ipc"] = "ipc"
# stat_map["system.cpu.cpi"] = "cpi"
# stat_map["system.cpu.cpi_total"] = "cpi_total"
# stat_map["system.cpu.dcache.overall_miss_rate::total"] = "dcache_miss_rate"
# stat_map["system.cpu.l2cache.overall_miss_rate::total"] = "l2cache_miss_rate"
# stat_map["system.l3.overall_miss_rate::total"] = "l3cache_miss_rate"
# stat_map["system.cpu.dcache.ReadReq_miss_rate::total"] = "dcache_read_miss_rate"
# stat_map["system.cpu.l2cache.ReadReq_miss_rate::total"] = "l2cache_read_miss_rate"
# stat_map["system.l3.ReadReq_miss_rate::total"] = "l3cache_read_miss_rate"
# stat_map["system.cpu.dcache.WriteReq_miss_rate::total"] = "dcache_write_miss_rate"
# stat_map["system.cpu.l2cache.WriteReq_miss_rate::total"] = "l2cache_write_miss_rate"
# stat_map["system.l3.WriteReq_miss_rate::total"] = "l3cache_write_miss_rate"

### We extract the following ramulator statistics

stat_map["ramulator.incoming_requests_per_channel"] = "incoming_requests"
# stat_map["ramulator.incoming_requests"] = "incoming_requests"
stat_map["ramulator.read_requests"] = "read_requests"
stat_map["ramulator.write_requests"] = "write_requests"
stat_map["ramulator.cpu_instructions_core_0"] = "cpu_insts"
stat_map["ramulator.cpu_cycles"] = "cpu_cycles"
stat_map["ramulator.maximum_bandwidth"] = "maximum_bandwidth"
stat_map["ramulator.maximum_internal_bandwidth"] = "maximum_internal_bandwidth"
stat_map["ramulator.maximum_link_bandwidth"] = "maximum_link_bandwidth"
stat_map["ramulator.read_bandwidth"] = "read_bandwidth"
stat_map["ramulator.write_bandwidth"] = "write_bandwidth"
stat_map["ramulator.row_hits"] = "row_hits"
stat_map["ramulator.row_misses"] = "row_misses"
stat_map["ramulator.row_conflicts"] = "row_conflicts"
stat_map["ramulator.read_row_hits"] = "read_row_hits"
stat_map["ramulator.read_row_misses"] = "read_row_misses"
stat_map["ramulator.read_row_conflicts"] = "read_row_conflicts"
stat_map["ramulator.write_row_hits"] = "write_row_hits"
stat_map["ramulator.write_row_misses"] = "write_row_misses"
stat_map["ramulator.write_row_conflicts"] = "write_row_conflicts"
stat_map["ramulator.read_latency_avg"] = "read_latency_avg"
stat_map["ramulator.queueing_latency_avg"] = "queueing_latency_avg"
stat_map["ramulator.request_packet_latency_avg"] = "request_packet_latency_avg"
stat_map["ramulator.response_packet_latency_avg"] = "response_packet_latency_avg"
stat_map["ramulator.maximum_bandwidth"] = "maximum_bandwidth"
stat_map["ramulator.maximum_internal_bandwidth"] = "maximum_internal_bandwidth"
stat_map["ramulator.ramulator_active_cycles"] = "DRAM_active_cycles"

# multidim_stats.append("ramulator.incoming_requests_per_channel::")
# multidim_stats.append("ramulator.req_queue_length_sum_")
# multidim_stats.append("ramulator.read_req_queue_length_sum_")
# multidim_stats.append("ramulator.write_req_queue_length_sum_")
#
stat_map["ramulator.serving_requests_"] = "total_serving_requests"
stat_map["ramulator.active_cycles_"] = "total_active_cycles"

multidim_stats.append("ramulator.serving_requests_")
multidim_stats.append("ramulator.active_cycles_")
#
# multidim_stats.append("ramulator.total_concurrent_request")
# multidim_stats.append("ramulator.multireq_memory_latency_sum_")

# some second level stats
#
# ipc
# row_hit_rate
# row_miss_rate
# row_conflict_rate
# BLP
# effective_BLP
# DRAM_latency_avg # access + transfer latency
# bandwidth
# bandwidth_utilization
# MPKI
#
# Wow, I think that would be all we have for now!

class Stats(object):

  def is_multidim_stats(self, full_name):
    for partial_name in multidim_stats:
      if partial_name in full_name:
        return partial_name
    return ""

  def get_mem_stats(self, name):
    if name in self.mem_stats:
      return self.mem_stats[name]
    else:
      return 0

  def __init__(self, fname):
    print fname
    self.stats = dict()
    self.mem_stats = dict()
    self.sys_stats = dict()
    if os.path.isfile(fname):
      f = open(fname, "r")
    else:
      return
    # parse statistics in output file
    for s in f:
      s = s.split(" ")
      s = filter(None, s)[:2]
      # not statistics
      if len(s) < 2 or "-" in s[0]:
        continue
      if s[0] == "#":
        continue
      value = float(s[1])
      if stat_map.has_key(s[0]):
        stat_name = stat_map[s[0]]
        if s[0].split(".")[0] == "ramulator":
          self.mem_stats[stat_name] = value
        else:
          self.sys_stats[stat_name] = value
      else:
        partial_name = self.is_multidim_stats(s[0])
        if not partial_name:
          continue
        indices = s[0][len(partial_name):].split("_")
        stat_name = stat_map[partial_name]
        value = float(s[1])
        if s[0].split(".")[0] == "ramulator":
          if self.mem_stats.has_key(stat_name) and self.mem_stats[stat_name].has_key(len(indices) - 1):
            self.mem_stats[stat_name][len(indices) - 1].append((indices, value))
          elif not self.mem_stats.has_key(stat_name):
            self.mem_stats[stat_name] = {len(indices) - 1 : [(indices, value)]}
          else:
            self.mem_stats[stat_name][len(indices) - 1] = [(indices, value)]
        else:
          assert(False and "Wow only ramulator has multidim stats!")
    # preprocess second-level statistics
  # ipc
    self.mem_stats["ipc"] = self.mem_stats["cpu_insts"] / self.mem_stats["cpu_cycles"]

    # row buffer locality
    self.mem_stats["row_hit_rate"] = self.mem_stats["row_hits"] / self.mem_stats["incoming_requests"]

    self.mem_stats["row_miss_rate"] = self.mem_stats["row_misses"] / self.mem_stats["incoming_requests"]

    self.mem_stats["row_conflict_rate"] = self.mem_stats["row_conflicts"] / self.mem_stats["incoming_requests"]

    # BLP
    # definition: average outstanding requests in DRAM per cycle
    ba_idx = len(self.mem_stats["total_serving_requests"]) - 1
    total_serving_requests_per_bank = self.mem_stats["total_serving_requests"][ba_idx]
    total_active_cycles_per_bank = self.mem_stats["total_active_cycles"][ba_idx]
    assert(len(total_serving_requests_per_bank) == len(total_active_cycles_per_bank))
    ba_num = len(total_serving_requests_per_bank)
    self.mem_stats["BLP"] = sum([t[1] for t in total_serving_requests_per_bank]) / (self.mem_stats["cpu_cycles"] * ba_num)
    if "DRAM_active_cycles" in self.mem_stats:
      self.mem_stats["effective_BLP"] = sum([t[1] for t in total_serving_requests_per_bank]) / self.mem_stats["DRAM_active_cycles"]
    else:
      print "Warning: stat DRAM_active_cycles doesn't exist"

    # latency
    self.mem_stats["DRAM_latency_avg"] = self.mem_stats["read_latency_avg"] - self.mem_stats["queueing_latency_avg"] - self.get_mem_stats("request_packet_latency_avg") - self.get_mem_stats("response_packet_latency_avg")

    # bandwidth
    self.mem_stats["bandwidth"] = self.mem_stats["write_bandwidth"] + self.mem_stats["read_bandwidth"]

    if "maximum_internal_bandwidth" in self.mem_stats:
      self.mem_stats["bandwidth_utilization"] = self.mem_stats["bandwidth"] / self.mem_stats["maximum_internal_bandwidth"]
    else:
      self.mem_stats["bandwidth_utilization"] = self.mem_stats["bandwidth"] / self.mem_stats["maximum_bandwidth"]

    # MPKI
    self.mem_stats["MPKI"] = self.mem_stats["incoming_requests"] * 1000 / self.mem_stats["cpu_insts"]
