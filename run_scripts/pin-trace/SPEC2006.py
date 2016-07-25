from subprocess import call
import sys

class SPEC2006:
  def __init__(self):
    self.single_threaded = ["mcf", "libquantum", "milc", "GemsFDTD", "bwaves", "soplex", "omnetpp", "gcc", "cactusADM", "zeusmp", "gobmk", "astar", "namd", "sphinx3", "sjeng", "bzip2", "hmmer", "perlbench", "h264ref", "calculix", "povray", "gamess"]
    self.workloads = [["milc", "GemsFDTD", "mcf", "libquantum"], ["bwaves", "omnetpp", "mcf", "libquantum"], ["libquantum", "bwaves", "soplex", "GemsFDTD"], ["soplex", "mcf", "omnetpp", "milc"], ["milc", "mcf", "GemsFDTD", "h264ref"], ["soplex", "omnetpp", "milc", "namd"], ["libquantum", "omnetpp", "bwaves", "povray"], ["libquantum", "mcf", "milc", "zeusmp"], ["omnetpp", "GemsFDTD", "cactusADM", "hmmer"], ["GemsFDTD", "mcf", "gamess", "zeusmp"], ["milc", "mcf", "bzip2", "h264ref"], ["bwaves", "soplex", "gamess", "namd"], ["omnetpp", "sjeng", "namd", "gcc"], ["GemsFDTD", "hmmer", "zeusmp", "astar"], ["GemsFDTD", "povray", "sphinx3", "calculix"], ["soplex", "zeusmp", "sphinx3", "gcc"], ["povray", "astar", "gobmk", "perlbench"], ["povray", "bzip2", "sphinx3", "cactusADM"], ["astar", "sjeng", "gcc", "cactusADM"], ["calculix", "namd", "perlbench", "gamess"]]
    self.expected_limit_insts = [
      1100000000, 1100000000, 1100000000, 1100000000,
      1100000000, 1100000000, 1100000000, 1100000000,
      1100000000, 1100000000, 1100000000, 1100000000,
      1100000000, 1100000000, 1100000000, 1100000000,
      1100000000, 1100000000, 1100000000, 1100000000
    ]
