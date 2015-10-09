#!/usr/bin/python

import random
import sys
import time
import tempfile
import subprocess
import psutil
import Queue
import thread
import shutil



class Sim(object):
  def __init__(self, _name, _trace):
    self.name = _name
    self.trace = _trace
  def argv(self, trc):
    return ''
  def parse_clk(self, stdout):
    return 0

class Ramulator(Sim):
  def __init__(self):
    super(Ramulator, self).__init__('Ramulator', 'ramulator')
  def argv(self, trc):
    return ['./ramulator', 'configs/DDR3-config.cfg', '--mode=mem', trc]
  def parse_clk(self, stdout):
    stdout = open("DDR3.stats")
    stdout.seek(0)
    for l in stdout.readlines():
      if 'ramulator.dram_cycles' in l:
        return int(l.split()[1])

class DRAMSim2(Sim):
  def __init__(self):
    super(DRAMSim2, self).__init__('DRAMSim2', 'ramulator')
  def argv(self, trc):
    base = '/Users/wky/code/DRAMSim2-2.2.2'
    return [base+'/DRAMSim', '-t', trc, '-s', base+'/system.ini', '-d', base+'/ini/DDR3-1600K-2Gbx8.ini']
  def parse_clk(self, stdout):
    stdout.seek(0)
    for l in stdout.readlines():
      if '== Pending Transactions' in l:
        b = l.find('(')
        e = l.find(')', b)
        clk = int(l[b+1:e])
    return clk

class USIMM(Sim):
  def __init__(self):
    super(USIMM, self).__init__('USIMM', 'usimm')
  def argv(self, trc):
    base = '/Users/wky/code/usimm-v1.3'
    return [base+'/bin/usimm', base+'/input/2Gb_x8.vi', trc]
  def parse_clk(self, stdout):
    stdout.seek(0)
    for l in stdout.readlines():
      if l.startswith('Total Simulation Cycles'):
        return int(l.split()[-1])

class DrSim(Sim):
  def __init__(self):
    super(DrSim, self).__init__('DrSim', 'drsim')
  def argv(self, trc):
    base = '/Users/wky/code/drsim'
    return [base+'/drsim', '--config', base+'/configs/ddr3-1600.cfg', trc]
  def parse_clk(self, stdout):
    stdout.seek(0)
    for l in stdout.readlines():
      if l.startswith('Simulation finished'):
        return int(l.split()[-1])

class NVMain(Sim):
  def __init__(self):
    super(NVMain, self).__init__('NVMain', 'nvmain')
  def argv(self, trc):
    base = '/Users/wky/code/nvmain-0f076410a356'
    return [base+'/nvmain.fast', base+'/Config/2D_DRAM_example.config', trc, '2000000000']
  def parse_clk(self, stdout):
    stdout.seek(0)
    for l in stdout.readlines():
      if l.startswith('Exiting at cycle'):
        return int(l.split()[3])

def gen_random(cb, n, rw, s, bits):
  l = s/64
  b = n/l
  for i in range(b):
    base = random.getrandbits(bits) & 0xffffffffffc0
    r = bool(random.random() < rw)
    for j in range(l):
      cb(base+j*64, r, l*i+j)

def gen_stream(cb, n, rw):
  r = int(n * rw)
  w = n - r
  for i in range(r):
    cb(i*64, True, i)
  for i in range(w):
    cb((r+i)*64, False, r+i)

# def collect_res(p, sim, ofile, res, q):
#   proc = psutil.Process(p.pid)
#   t, mem = 0, 0
#   while p.poll() is None:
#     try:
#       mem = max(mem, proc.memory_info()[0]) # rss
#       t = sum(proc.cpu_times())
#     except psutil.AccessDenied, e: print "======== Oops %s %d failed ===============" % (sim.name, p.pid)
#     time.sleep(0.1)
#   print '%s(%d) finished.' % (sim.name, p.pid)
#   clk = sim.parse_clk(ofile)
#   res[sim.name] = (clk, t, mem)
#   q.get()
#   q.task_done()

def main(n_reqs, rw, rec):
  trace_names = ['ramulator', 'usimm', 'drsim', 'nvmain']
  def make_cb(files):
    def real_cb(addr, rw, i):
      files['ramulator'].write('0x%x %s\n' % (addr, 'R' if rw else 'W'))
      files['usimm'].write('0 %s 0x%x %s\n' % ('R' if rw else 'W', addr, '0x0' if rw else ''))
      files['drsim'].write("0x%x %s %d\n" % (addr, 'READ' if rw else 'WRITE', i))
      files['nvmain'].write("%d %s 0x%x %s 0\n" % (i, 'R' if rw else 'W', addr, '0' * 128))
    return real_cb

  s = 64
  traces = []
  
  tmps = {name: tempfile.NamedTemporaryFile() for name in trace_names}
  gen_random(make_cb(tmps), n_reqs, rw, s, 31)
  for f in tmps.itervalues():
    f.file.seek(0)
  traces.append(tmps)
  print 'Random trace created'

  tmps = {name: tempfile.NamedTemporaryFile() for name in trace_names}
  gen_stream(make_cb(tmps), n_reqs, rw)
  for f in tmps.itervalues():
    f.file.seek(0)
  traces.append(tmps)
  print 'Stream trace created'

  if rec:
      for name, tmpf in traces[0].iteritems():
        shutil.copy(tmpf.name, './%s-random.trace' % name)
      for name, tmpf in traces[1].iteritems():
        shutil.copy(tmpf.name, './%s-stream.trace' % name)

  sims = [Ramulator(), DRAMSim2(), USIMM(), DrSim(), NVMain()]
  cnt = len(traces) * len(sims)

  blackhole = open('/dev/null', 'w')
  results = []
  for v in traces:
    res_dict = {}
    for sim in sims:
      tmp = tempfile.NamedTemporaryFile()
      p = subprocess.Popen(sim.argv(v[sim.trace].name), stdout=tmp.file, stderr=blackhole)
      print 'Starting %s %d' % (sim.name, p.pid)
      proc = psutil.Process(p.pid)
      t, mem = 0, 0
      while p.poll() is None:
        try:
          mem = max(mem, proc.memory_info()[0]) # RSS on mac
          t = sum(proc.cpu_times())
        except: print "======== Oops monitoring %s %d failed ===============" % (sim.name, p.pid)
        time.sleep(0.1)
      print '%s(%d) finished.' % (sim.name, p.pid)
      clk = sim.parse_clk(tmp.file)
      res_dict[sim.name] = (clk, t, mem)
      tmp.file.close()
    results.append(res_dict)
  blackhole.close()
  print results


if __name__ == '__main__':
  if len(sys.argv) < 3: print 'test_ddr3.py <n-requests> <read proportion> [record]'
  else: main(int(sys.argv[1]), float(sys.argv[2]), (len(sys.argv) > 3 and sys.argv[3] == 'record'))


