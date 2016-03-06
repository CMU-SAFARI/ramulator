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
stat_map["ramulator.read_latency_ns_avg"] = "read_latency_ns_avg"
stat_map["ramulator.queueing_latency_ns_avg"] = "queueing_latency_ns_avg"
stat_map["ramulator.request_packet_latency_ns_avg"] = "request_packet_latency_ns_avg"
stat_map["ramulator.response_packet_latency_ns_avg"] = "response_packet_latency_ns_avg"
stat_map["ramulator.maximum_bandwidth"] = "maximum_bandwidth"
stat_map["ramulator.maximum_internal_bandwidth"] = "maximum_internal_bandwidth"
stat_map["ramulator.ramulator_active_cycles"] = "DRAM_active_cycles"

# multidim_stats.append("ramulator.incoming_requests_per_channel::")
# multidim_stats.append("ramulator.req_queue_length_sum_")
# multidim_stats.append("ramulator.read_req_queue_length_sum_")
# multidim_stats.append("ramulator.write_req_queue_length_sum_")
#
stat_map["ramulator.record_cycs_core_"] = "cpu_cycles"
stat_map["ramulator.record_insts_core_"] = "cpu_insts"
stat_map["ramulator.serving_requests_"] = "total_serving_requests"
stat_map["ramulator.active_cycles_"] = "total_active_cycles"

multidim_stats.append("ramulator.record_cycs_core_")
multidim_stats.append("ramulator.record_insts_core_")
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

  def get_mem_stats(self, mem_stats, name):
    if name in mem_stats:
      return mem_stats[name]
    else:
      return 0

  def init_from_file(self, f):
    # parse statistics in output file
    mem_stats = dict()
    sys_stats = dict()
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
          mem_stats[stat_name] = value
        else:
          sys_stats[stat_name] = value
      else:
        partial_name = self.is_multidim_stats(s[0])
        if not partial_name:
          continue
        indices = s[0][len(partial_name):].split("_")
        stat_name = stat_map[partial_name]
        value = float(s[1])
        if s[0].split(".")[0] == "ramulator":
          if mem_stats.has_key(stat_name) and mem_stats[stat_name].has_key(len(indices) - 1):
            mem_stats[stat_name][len(indices) - 1].append((indices, value))
          elif not mem_stats.has_key(stat_name):
            mem_stats[stat_name] = {len(indices) - 1 : [(indices, value)]}
          else:
            mem_stats[stat_name][len(indices) - 1] = [(indices, value)]
        else:
          assert(False and "Wow only ramulator has multidim stats!")
  # preprocess second-level statistics
    if len(mem_stats) == 0:
      return

    try:
    # max_cpu_cycles
      mem_stats["max_cpu_cycles"] = max([c[1] for c in mem_stats["cpu_cycles"][0]])
    except KeyError as detail:
      print "Ignore KeyError: ", detail
      pass

    try:
    # ipc
      mem_stats["ipc"] = []
      for cpu_insts,cpu_cycles in zip(mem_stats["cpu_insts"][0], mem_stats["cpu_cycles"][0]):
        mem_stats["ipc"].append(cpu_insts[1]/cpu_cycles[1])
    except KeyError as detail:
      print "Ignore KeyError: ", detail
      pass

    # weithed_speedup
    if len(mem_stats["ipc"]) > 1:
      try:
        ref_ipcs = [s["ipc"][0] for s in self.ref_mem_stats]
        num_of_core = len(mem_stats["ipc"])
        weighted_speedup = 0
        assert(len(ref_ipcs) == len(mem_stats["ipc"]))
        for ipc, ref_ipc in zip(mem_stats["ipc"], ref_ipcs):
          weighted_speedup += ipc / ref_ipc
        mem_stats["weighted_speedup"] = weighted_speedup
      except KeyError as detail:
        print "Ignore KeyError: ", detail
        pass

    try:
      # row buffer locality
      mem_stats["row_hit_rate"] = mem_stats["row_hits"] / mem_stats["incoming_requests"]

      mem_stats["row_miss_rate"] = mem_stats["row_misses"] / mem_stats["incoming_requests"]

      mem_stats["row_conflict_rate"] = mem_stats["row_conflicts"] / mem_stats["incoming_requests"]
    except KeyError as detail:
      print "Ignore KeyError: ", detail
      pass

    try:
      # BLP
      # definition: average outstanding requests in DRAM per cycle
      ba_idx = len(mem_stats["total_serving_requests"]) - 1
      total_serving_requests_per_bank = mem_stats["total_serving_requests"][ba_idx]
      total_active_cycles_per_bank = mem_stats["total_active_cycles"][ba_idx]
      assert(len(total_serving_requests_per_bank) == len(total_active_cycles_per_bank))
      ba_num = len(total_serving_requests_per_bank)
      mem_stats["BLP"] = sum([t[1] for t in total_serving_requests_per_bank]) / (mem_stats["max_cpu_cycles"] * ba_num)
      if "DRAM_active_cycles" in mem_stats:
        mem_stats["effective_BLP"] = sum([t[1] for t in total_serving_requests_per_bank]) / mem_stats["DRAM_active_cycles"]
      else:
        print "Warning: stat DRAM_active_cycles doesn't exist"
    except KeyError as detail:
      print "Ignore KeyError: ", detail
      pass

    try:
      # latency
      if not ("request_packet_latency_avg" and "response_packet_latency_avg") in mem_stats:
        mem_stats["request_packet_latency_avg"] = 0
        mem_stats["response_packet_latency_avg"] = 0
      mem_stats["DRAM_latency_avg"] = mem_stats["read_latency_avg"] - mem_stats["queueing_latency_avg"] - self.get_mem_stats(mem_stats, "request_packet_latency_avg") - self.get_mem_stats(mem_stats, "response_packet_latency_avg")
      # latency_ns
      if not ("request_packet_latency_ns_avg" and "response_packet_latency_ns_avg") in mem_stats:
        mem_stats["request_packet_latency_ns_avg"] = 0
        mem_stats["response_packet_latency_ns_avg"] = 0
      mem_stats["DRAM_latency_ns_avg"] = mem_stats["read_latency_ns_avg"] - mem_stats["queueing_latency_ns_avg"] - self.get_mem_stats(mem_stats, "request_packet_latency_ns_avg") - self.get_mem_stats(mem_stats, "response_packet_latency_ns_avg")
    except KeyError as detail:
      print "Ignore KeyError: ", detail
      pass

    try:
      # bandwidth
      mem_stats["bandwidth"] = mem_stats["write_bandwidth"] + mem_stats["read_bandwidth"]

      if "maximum_internal_bandwidth" in mem_stats:
        mem_stats["bandwidth_utilization"] = mem_stats["bandwidth"] / mem_stats["maximum_internal_bandwidth"]
      else:
        mem_stats["bandwidth_utilization"] = mem_stats["bandwidth"] / mem_stats["maximum_bandwidth"]
    except KeyError as detail:
      print "Ignore KeyError: ", detail
      pass

    try:
      # MPKI
      mem_stats["MPKI"] = mem_stats["incoming_requests"] * 1000 / mem_stats["cpu_insts"][0][0][1]
    except KeyError as detail:
      print "Ignore KeyError: ", detail
      pass
    return (mem_stats, sys_stats)

  def __init__(self, target_fname, ref_fname_list = None):
    self.ref_mem_stats = []
    self.ref_sys_stats = []
    if os.path.isfile(target_fname):
      target_fdesc = open(target_fname, "r")
    else:
      print "target file name %s doesn't exist, return earlier" % target_fname
      return
    if not ref_fname_list is None:
      for ref_fname in ref_fname_list:
        if os.path.isfile(ref_fname):
          ref_fdesc = open(ref_fname, "r")
          ref_mem_stats, ref_sys_stats = self.init_from_file(ref_fdesc)
          self.ref_mem_stats.append(ref_mem_stats)
          self.ref_sys_stats.append(ref_sys_stats)

    self.mem_stats, self.sys_stats = self.init_from_file(target_fdesc)

