#!/usr/bin/python
import os
import matplotlib
matplotlib.use('Agg')
from matplotlib import pyplot as plt

import numpy as np
import operator

def main():
  draw_standards()

def draw_standards():
  std_names = ['DDR3', 'DDR4', 'SALP', 'LPDDR3', 'LPDDR4','GDDR5', 'HBM', 'WIO', 'WIO2']
  colors = ['0.9', '0.6', '0.8', '0.55', '0.7', '0.5', '0.6', '0.4', '0.5']

  orig_data = []
  res_cnt = 0
  for fn in os.listdir('results'):
    if fn[0] == '.': continue
    with open('results/' + fn, 'r') as f:
      orig_data.append(map(float, f.readlines()[1:]))
    res_cnt += 1

  orig_data = np.swapaxes(orig_data, 0, 1)
  means = map(lambda a: reduce(operator.mul, a) ** (1./res_cnt), orig_data)
  minmax = np.swapaxes([[m-min(a), max(a)-m] for m, a in zip(means, orig_data)], 0, 1)
  pos = np.arange(9)
  #fig, ax = plt.subplots(figsize=(1.8, 1.2))
  fig, ax = plt.subplots(figsize=(3.5, 1.2))
  plt.grid(axis='y', zorder=-3, lw=0.5, ls=':')
  #ax.axhline(1.0, ls='-', lw=0.75, color='black')
  yticks = [0.0, 0.5, 1.0, 1.5, 2.0]
  plt.yticks(yticks, map(str, yticks), size=6)
  plt.ylim(0, 2.0)
  plt.xlim(-0.15, 9.15)
  # ax.set_yticklabels([])
  ax.set_ylabel('Gmean IPC \n (Normalized to DDR3)', size=6, 
      multialignment='center')

  bars = ax.bar(pos+0.15, means, yerr=minmax, ecolor='red', 
      error_kw={'zorder':-1, 'barsabove':True}, 
      width=0.7, color=colors, linewidth=0.5, zorder=-2)

  for i, m in enumerate(means):
    if m + minmax[1][i] > 2.0: # error bar outside plot
      ax.text(pos[i] + 0.525, 1.85, "max=%.2f" % (m + minmax[1][i]), size=5)

  for i in xrange(9):
    ax.text(pos[i]+0.5, means[i]+0.1, '%.2f' % means[i],
        ha='center', size=6, zorder=10)

  ax.get_xaxis().tick_bottom()
  ax.get_yaxis().tick_left()
  ax.set_xticklabels([])
  ax.tick_params('both', length=0, which='major')

  ax.spines['bottom'].set_linewidth(0.5)
  ax.spines['top'].set_linewidth(0.5)
  ax.spines['left'].set_linewidth(0.5)
  ax.spines['right'].set_linewidth(0.5)

  for i in xrange(9):
    ax.text(pos[i]+0.525, 0.1, std_names[i], ha='center', va='bottom', 
        rotation='vertical', size=6, zorder=10)

  fig.savefig('std.pdf', bbox_inches='tight', pad_inches=0.05)

if __name__ == '__main__':
  main()

