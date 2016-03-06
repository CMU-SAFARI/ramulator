import stats_analyzer as sa
import numbers

class workload_stats(object):
  """ statistics for each DRAM with a certain workload
  """

  def __init__(self, configs):
    """ initialize this object
    """
    self.workload = configs.workload
    self.workload_dir = configs.workload_dir
    self.DRAM_list = configs.DRAM_list
    self.ref_DRAM = configs.ref_DRAM
    print "%s as reference DRAM standards" % self.ref_DRAM
    self.single_threaded_workload_list = configs.single_threaded_workload_list # list of workloads for each core
    self.w_stats = dict()
    self.ref_stats = None # DDR3 statistics, for normalization and reorder

    for DRAM in self.DRAM_list:
      if self.single_threaded_workload_list is None:
        s = sa.Stats(self.workload_dir + "/" + self.workload + "/" + DRAM + ".stats")
      else:
        s = sa.Stats(self.workload_dir + "/" + self.workload + "/" + DRAM + ".stats", [self.workload_dir + "/" + workload_name + "/" + self.ref_DRAM + ".stats" for workload_name in self.single_threaded_workload_list])
      self.w_stats[DRAM] = s
      if DRAM == self.ref_DRAM:
        self.ref_stats = s

  def append_2D_to(self, attrnames, filedesc):
    """ append scalar statistics to file
    :type filedesc: file descriptor
    """
    if self.single_threaded_workload_list is None: # single-threaded
      assert(len(self.ref_stats.mem_stats["ipc"]) == 1)
      workload_and_feature = "_".join([self.workload, "MPKI", "%.4f" % self.ref_stats.mem_stats["MPKI"], "ipc", "%.4f" % sum(self.ref_stats.mem_stats["ipc"])])
    else: # multiprogram
      workload_and_feature = "_".join([self.workload, "MPKI", "%.4f" % sum([s["MPKI"] for s in self.ref_stats.ref_mem_stats]), "weighted-speedup", "%.4f" % self.ref_stats.mem_stats["weighted_speedup"]])

    filedesc.write(workload_and_feature + "," + ",".join(self.DRAM_list) + "\n")
    for attrname in attrnames:
      values = []
      for DRAM in self.DRAM_list:
        if isinstance(self.w_stats[DRAM].mem_stats[attrname], list):
          assert(not attrname == "ipc" or len(self.w_stats[DRAM].mem_stats[attrname]) == 1)
          values.append(sum(self.w_stats[DRAM].mem_stats[attrname]))
        else:
          values.append(self.w_stats[DRAM].mem_stats[attrname])
      filedesc.write(attrname + "," + ",".join([("%.4f" % v) for v in values]) + "\n")

  def append_1D_to(self, attrnames, filedesc):
    """ append scalar statistics to file (in a format similar to relational database)
    """
    if self.single_threaded_workload_list is None: # single-threaded
      assert(len(self.ref_stats.mem_stats["ipc"]) == 1)
      ref_MPKI=self.ref_stats.mem_stats["MPKI"]
      feature = "_".join(["MPKI", "%.4f" % ref_MPKI, "ipc", "%.4f" % sum(self.ref_stats.mem_stats["ipc"])])
      workload_and_feature = "_".join([self.workload, feature])
    else: # multiprogram
      ref_MPKI=sum([s["MPKI"] for s in self.ref_stats.ref_mem_stats])
      feature = "_".join(["MPKI", "%.4f" % ref_MPKI, "weighted-speedup", "%.4f" % self.ref_stats.mem_stats["weighted_speedup"]])
      workload_and_feature = "_".join([self.workload, feature])

    for attrname in attrnames:
      for DRAM in self.DRAM_list:
        if isinstance(self.w_stats[DRAM].mem_stats[attrname], list):
          assert(not attrname == "ipc" or len(self.w_stats[DRAM].mem_stats[attrname]) == 1)
          value = sum(self.w_stats[DRAM].mem_stats[attrname])
        else:
          value = self.w_stats[DRAM].mem_stats[attrname]
        filedesc.write(",".join([attrname, "%.4f" % value, DRAM, self.workload, feature, workload_and_feature, "%.4f" % ref_MPKI]) + "\n")

  def normalize(self, attrname):
    """ normalize scalar statistics
    """
    norm_attrname = "normalized_" + attrname
    if isinstance(self.ref_stats.mem_stats[attrname], numbers.Number):
      for (DRAM, s) in self.w_stats.iteritems():
        self.w_stats[DRAM].mem_stats[norm_attrname] = s.mem_stats[attrname] / self.ref_stats.mem_stats[attrname]
      return
    if isinstance(self.ref_stats.mem_stats[attrname], list):
      for (DRAM, s) in self.w_stats.iteritems():
        self.w_stats[DRAM].mem_stats[norm_attrname] = [ipc/self.ref_stats.mem_stats[attrname][0] for ipc in s.mem_stats[attrname]]
