from subprocess import call
import sys

class Rowclone:
  def __init__(self):
    self.single_threaded = [
                  "apache2.1B-rc",
                  "compile-rc",
                  "memcached.1B-rc",
                  "bootup.1B-rc",
                  "filecopy.1B-rc",
                  "mysql.1B-rc",
                  "clonevm_running-rc",
                  "forkset-rc",
                  "shell-rc"
                ]
# TODO: add expectd_limit for each group
    self.workloads = []


