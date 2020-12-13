import random
import sys

if len(sys.argv) < 5:
  print "Usage: gen_trace.py [random_coefficient] [trace_len] [cpu_inst] [write_coefficient] [output]"
  sys.exit()

tx_bits = 6 # 64 bytes
tx_size = 1<<tx_bits
mem_footprint_size = 4*1024*1024

def init_addr_list(addr_list, max_address, tx_size):
  item_num = max_address / tx_size
  for i in range(item_num):
    addr_list.append(i * tx_size)

addr_list = []
random_coefficient = float(sys.argv[1])
trace_len = int(sys.argv[2])
cpu_inst = int(sys.argv[3])
write_coefficient = float(sys.argv[4])
output_f = open(sys.argv[5], "w")

init_addr_list(addr_list, mem_footprint_size, tx_size)

random.seed(0)

ridx = 0
widx = 0
trace = []
mem_count = trace_len

while mem_count > 0:
  if random.random() < write_coefficient:
    trace.append(" ".join([str(v) for v in [cpu_inst, addr_list[ridx], addr_list[widx]]]))
    mem_count = mem_count - 2
  else:
    trace.append(" ".join([str(v) for v in [cpu_inst, addr_list[ridx]]]))
    mem_count = mem_count - 1

  if random.random() < random_coefficient:
    ridx = random.randint(0, len(addr_list)-1)
    widx = random.randint(0, len(addr_list)-1)
  else:
    ridx = (ridx + 1) % len(addr_list)
    widx = (widx + 1) % len(addr_list)

output_f.write("\n".join(trace) + "\n")
output_f.close()
