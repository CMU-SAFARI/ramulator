# Trace Generator
This is an Intel Pin-based tool for generating memory access traces of real workloads. The traces are intended to be used with Ramulator but they can be useful for other purposes too.

## Overview
### Motivation
To understand the behavior of a processor, cycle-accurate simulators are used frequently with various benchmarks. But running benchmarks on cycle-accurate simulators require too much time. The motivation of this trace generator is to generate relatively short but representative traces to get accurate simulation results in less time with an efficient way.

### How It Works
The goal is to find workload phases that are most representative of general characteristics of a given workload. By generating traces for only these representative phases rather than the entire workload, it is possible to quickly simulate the overall characteristics of the workload.

To determine which phases of a workload represent the dominant workload characteristics, we use [Simpoint](https://cseweb.ucsd.edu/~calder/simpoint/).
Simpoint performs offline _phase analysis_ to collect information about workload phases and determine the representative ones.

By default, our trace generator collects a memory access trace for the entire workload. In fast mode, the tool generates a single trace based on the workload phase with the highest weight (according to Simpoint). However, the user can specify a coverage target, which will result in generating multiple traces based on several phases with the highest weights such that the total weight equals or exceeds the coverage target.

Using the generated traces, Ramulator simulates a workload in less time than required for simulating the entire workload.

### Prerequisites

 - Tool requires a **C++11** compiler
    - gcc version >= 5.5.0
    - c++ version >= 5.5.0


 - **Pin3.7** or later versions of Pin
    - Tested version: Pin 3.7 Kit: 97619


 - **Perl** is installed by default in most Linux distributions.
    - You can check your Perl version with `perl --version`.
    - Tested version: v5.22.1


## Getting Started

0. **Warning** This tool requires root permissions.

1.  set **PIN_ROOT** environment variable before using the tool

    `$ export PIN_ROOT=<path to your pin kit>`


2. change the pin kit's makefile to compile with C++11: (You only need to do this once.)
  - `cd $PIN_ROOT/source/tools/Config`
  - add `-std=c++11 -faligned-new` to `makefile.unix.config:109`.
  - New line should look like this:
    `TOOL_CXXFLAGS_NOOPT := -std=c++11 -faligned-new -Wall -Werror -Wno-unknown-pragmas -D__PIN__=1 -DPIN_CRT=1`


3. Build tools:
  - `./build.sh`
  - Script will build Simpoint and pintools.


4. `./tracegenerator.sh <options> -- <program_name>`
  - To force re-compilation of the trace generator, use the `-force` option.

### Example Run
Following is an example run, it collects the memory access trace of the most weighted phase of the workload "ls -al" with instruction cache and address translation disabled.

```
$ cd tracegenerator
$ export PIN_ROOT=<path of your pin kit>
$ ./tracegenerator.sh -ifetch off -paddr off -fast -intervalsize 10000 -- ls -al
root
kernel.randomize_va_space = 0 # disabling ASLR
/home/nisa/trace_generator/pin/pin -t src/PinPoints/obj-intel64/isimpoint.so -slice_size 100000 -o temp-tg18363/tg18363 -- ls -al # running pinpoints' isimpoint tool to get frequency vectors for ls -al
... # output of ls -al
Simpoint3.2/bin/simpoint -maxK 30 -saveSimpoints temp-tg18363/tg18363.pts -saveSimpointWeights temp-tg18363/tg18363.w  -saveLabels temp-tg18363/tg18363.lbl -loadFVFile temp-tg18363/tg18363.T.0.bb > temp-tg18363/tg18363.simpoint.log # running Simpoint with frequency vector file to get simpoints
Simpoint log can be found at temp-tg18363/tg18363.simpoint.log
perl src/PinPoints/bin/ppgen.3 temp-tg18363/tg18363 temp-tg18363/tg18363.T.0.bb temp-tg18363/tg18363.pts temp-tg18363/tg18363.w temp-tg18363/tg18363.lbl 0 # generating execution counts and program counters for the simpoints
Slice Size: 100000
Writing [temp-tg18363/tg18363.1.pp]
/home/nisa/trace_generator/pin/pin -t src/obj-intel64/gettrace.so  -ifetch 0 -paddr 0 -fast 1 -ppoints temp-tg18363/tg18363.1.pp  -- ls -al # running the tool collecting traces
...
Temporary files are under temp-tg18363 directory.
Trace generation took 5 seconds.
kernel.randomize_va_space = 2 # enabling ASLR

```



## Cache Simulation
### Motivation
Cache behavior can be simulated while generating a trace and filtering the instructions that hit in the cache will result in a shorter trace to simulate in Ramulator. By default, the trace generator emulates a first level (L1) cache and generates L1-filtered traces. The trace generator provides several options to change the emulated cache configuration.

Cache options can be modified in two ways:

1. From the config file:
  - An example config file can be found at src/Cache.cfg.
  - After the `level = <level>` the other options (size, associativity and block size) can be set.

2. From command line:
  - `l1_size <l1_size>` overrides the size of the first level cache
  - `l1_assoc <l1_assoc>` overrides number of sets in the first level cache
  - `l1_block_size <l1_block_size>` overrides block size in the first level cache
  - ... ( can be used with l2_... and l3_... for the other levels of caches. )

  Data cache can be disabled by `-dcache off` command line argument.

## Instruction Fetching and Instruction Cache Configurations
By default instruction cache is enabled in the tool. This will cause instruction fetches to be included in the trace as read requests. Normally, it should be desired to including instruction fetches in the trace as it would more accurately represent the actual memory access characteristics of a workload.

Instruction cache configurations can be changed by:
1. Changing the src/Cache.cfg file.
	`level = -1` indicates that the following options are selected for instruction cache.
2. Options from command line:
	`ic_size, ic_assoc, ic_block_size, ic_mshr_num` options can be used for changing these settings.

Instruction fetching accesses can be filtered by using `-ifetch off` argument.
Instruction cache can be disabled by `-icache off` argument.

## Data Dependencies

Prior CPU-driven traces that Ramulator uses typically do not include information to indicate dependency between two requests. For example, _read_req1_ may fetch an address where _read_req2_ is supposed to read from. This trace generator _optionally_ includes data dependency information in the trace. The tool tracks data dependencies within a fixed time window. The format of a trace that includes data dependency information is:

  *{sequence number}* &nbsp; *{instruction type}* &nbsp; : &nbsp; *{dependency list}*
 - Sequence number is a number given to each instruction. (Default size of the window is 128. Therefore a sequence number is a number from 0 to 127.)
 - Instruction type can be: READ, WRITE, COMP.
 - Dependency list is a list of sequence numbers.

 To generate this type of traces use `-mode datadep`

## Simpoint options
[Simpoint](https://cseweb.ucsd.edu/~calder/simpoint/) gives information about the workload based on phase analysis. The output includes representative points (i.e., phases) with corresponding weights. (Simpoint output files can be found under the generated temp-tg*{PID}* or temp-*{given_trace_file_name}* directory after running the trace generator.)  

1. `-fast` option: Chooses the point with the maximum weight.
2. `-cvg .N` option: Generates points with the sum of the weights equals to 0.N coverage. (For example: for 0.99 coverage you should give -cvg .99)
3. default option: Generates trace of the whole program. Simpoint outputs are ignored.

## Other Command Line Options
1. **Trace Output File:**
`-t <outputfile>`

2. **Config File:**
`-c <configfile>`

3. **Trace Type:** [default is cpu]
`-mode cpu/mem/datadep`

4. **Address** [default is physical address]
`-paddr off/on`

5. **Instruction Fetch Recording** [default is recorded]
`-ifetch off/on`

6. **Interval Size** [Default is "100000000"]
`-intervalsize <int>`

  - Interval size is used to generate fast and coverage traces and it fixes the number of instructions in a trace file.

  - **Warnings:**
    - Setting the interval size larger than the benchmark will not give any trace files. In that case the full simulation option should be selected.
    - Setting the interval size too small will result in smaller trace files, note that Simpoint analysis will take more time.

## How to Use Generated Traces

### Full Simulation Option

Simulation result is the overall result for full simulation option.

### Coverage Option

Overall result can be calculated using the Simpoint weights in temporary directory generated by the tool.
```
$ ./tracegenerator.sh -cvg .99 -t example -- ls

  ...

  Writing [temp-example/example.1.pp]
  Temporary files are under temp-example directory.
  ...

$ cat temp-example/example.1.pp

  ...

    ## Slice number corresponds to the trace file "example.<number>"
    ## Slice weight is given in "region <number> <weight>" line.

    ## For 11th slice tool generates "example.12"
    ## weight corresponding to 11th slice is "4.976"

  #Pinpoint= 11 Slice= 168378  Icount= 16837800000  Len= 100000
  region 11 4.976 1 1 100000 16837800000
  slice 168378 14 0.004876
  warmup_factor 0
  start 13 734095  0
  end   14 221620 0

    ## For 12th slice tool generates "example.12"
    ## Weight for 12th slice is "1.788"

  #Pinpoint= 12 Slice= 184612  Icount= 18461200000  Len= 100000
  region 12 1.788 1 1 100000 18461200000
  slice 184612 13 0.009210
  warmup_factor 0
  start 15 16624  0
  end   16 16668 0
  ...

  totalIcount 32466006352
  pinpoints 20
  endp

```

### Fast Option

Overall result can be calculated using the weight of the chosen slice again using the .pp file in the temporary files directory.


### Contributors

- Nisa Bostanci (@nisabostanci) (TOBB University of Economics and Technology)