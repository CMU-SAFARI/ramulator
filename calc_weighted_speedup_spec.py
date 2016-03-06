import sys
import stats_analyzer as sa
from subprocess import call

if (len(sys.argv) < 3):
  print "Usage: python calc_weighted_speedup.py single_threaded_stats multi_core_stats"
  sys.exit()

single_threaded_stats = sys.argv[1]
multicore_stats = sys.argv[2]

single_threaded_ipc = 0

# single_threaded_ipc
s = sa.Stats(single_threaded_stats)
if not len(s.mem_stats) == 0:
  single_threaded_ipc = s.mem_stats["ipc"][0]

assert(not single_threaded_ipc == 0)

s = sa.Stats(multicore_stats)
if not len(s.mem_stats) == 0:
  num_of_core = len(s.mem_stats["ipc"])
  ipc_ratio_sum = 0
  for j in range(num_of_core):
    ipc_ratio_sum += s.mem_stats["ipc"][j] / single_threaded_ipc
  print ipc_ratio_sum
