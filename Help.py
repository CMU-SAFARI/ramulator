#!/usr/bin/env python
import matplotlib.pyplot as plt; plt.rcdefaults()
import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
import csv
import sys

df = pd.read_csv("slowdown.csv")
slowdown = df['slowdown']
y_pos = np.arange(len(slowdown))
ids = df['trace']

# plt.scatter(ids, slowdown, color='gold')
plt.bar(y_pos, slowdown, align='center', alpha=0.5)
plt.xticks(y_pos, ids, rotation=30)


plt.xlabel('trace')
plt.ylabel('slowdown')

plt.show()
