from subprocess import call
import sys

class Hadoop:
  def __init__(self):
    self.single_threaded = [
                "grep-map0",
                "grep-reduce0",
                "wordcount-map0",
                "wordcount-reduce0",
                "sort-map0",
                ]
# TODO: add expectd_limit for each group
    self.workloads = [[
                "grep-map0",
                "grep-map1",
                "grep-map2",
                "grep-map3",
            ],
            [
                "wordcount-map0",
                "wordcount-map1",
                "wordcount-map2",
                "wordcount-map3",
            ],
            [
                "sort-map0",
                "sort-map1",
                "sort-map2",
                "sort-map3"
            ]]
    self.expected_limit_insts = [
      7121840399, 412596850, 22705986279, 1886863202
    ]


