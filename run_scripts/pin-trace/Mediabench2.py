from subprocess import call
import sys

class Mediabench2:
  def __init__(self):
    self.single_threaded =["h264-decode",
                    "jp2-decode",
                    "h264-encode",
                    "jp2-encode",
                    ]
    self.workloads = []
