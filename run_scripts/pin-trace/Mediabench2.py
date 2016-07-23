from subprocess import call
import sys

class Mediabench2:
  def __init__(self):
    self.single_threaded =["mediabench2-h264-decode",
                    "mediabench2-jp2-decode",
                    "mediabench2-h264-encode",
                    "mediabench2-jp2-encode",
                    ]
    self.workloads = []
