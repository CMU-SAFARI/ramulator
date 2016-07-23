from subprocess import call
import sys

class Redis:
  def __init__(self):
    self.single_threaded = ["ycsb-workloada-bgsave", "ycsb-workloadc-server", "ycsb-workloada-server", "ycsb-workloadd-server", "ycsb-workloadb-server", "ycsb-workloade-server"]
    self.workloads = [
            ["ycsb-workloada-server", "ycsb-workloadb-server", "ycsb-workloadc-server", "ycsb-workloadd-server"],
            ["ycsb-workloada-server", "ycsb-workloadb-server", "ycsb-workloadc-server", "ycsb-workloade-server"],
            ["ycsb-workloada-server", "ycsb-workloadb-server", "ycsb-workloadd-server", "ycsb-workloade-server"],
            ["ycsb-workloada-server", "ycsb-workloadc-server", "ycsb-workloadd-server", "ycsb-workloade-server"],
            ["ycsb-workloadb-server", "ycsb-workloadc-server", "ycsb-workloadd-server", "ycsb-workloade-server"]]

