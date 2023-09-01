We have released an updated version of Ramulator, called [Ramulator 2.0](https://github.com/CMU-SAFARI/ramulator2), in August 2023. Ramulator 2.0 is easier to use, extend, and modify. It also has support for the latest DRAM standards at the time (e.g., DDR5, LPDDR5, HBM3 GDDR6). We suggest that you use Ramulator 2.0 and welcome your feedback and bug/issue reports.

# Ramulator: A DRAM Simulator

Ramulator is a fast and cycle-accurate DRAM simulator \[1, 2\] that supports a
wide array of commercial, as well as academic, DRAM standards:

- DDR3 (2007), DDR4 (2012)
- LPDDR3 (2012), LPDDR4 (2014)
- GDDR5 (2009)
- WIO (2011), WIO2 (2014)
- HBM (2013)
- SALP \[3\]
- TL-DRAM \[4\]
- RowClone \[5\]
- DSARP \[6\]

The initial release of Ramulator is described in the following paper:
>Y. Kim, W. Yang, O. Mutlu.
>"[**Ramulator: A Fast and Extensible DRAM Simulator**](https://people.inf.ethz.ch/omutlu/pub/ramulator_dram_simulator-ieee-cal15.pdf)".
>In _IEEE Computer Architecture Letters_, March 2015.

For information on new features, along with an extensive memory characterization using Ramulator, please read:
>S. Ghose, T. Li, N. Hajinazar, D. Senol Cali, O. Mutlu.
>"[**Demystifying Complex Workload–DRAM Interactions: An Experimental Study**](https://people.inf.ethz.ch/omutlu/pub/Workload-DRAM-Interaction-Analysis_sigmetrics19_pomacs19.pdf)".
>In _Proceedings of the ACM International Conference on Measurement and Modeling of Computer Systems (SIGMETRICS)_, June 2019 ([slides](https://people.inf.ethz.ch/omutlu/pub/Workload-DRAM-Interaction-Analysis_sigmetrics19-talk.pdf)).
>In _Proceedings of the ACM on Measurement and Analysis of Computing Systems (POMACS)_, 2019.

[\[1\] Kim et al. *Ramulator: A Fast and Extensible DRAM Simulator.* IEEE CAL
2015.](https://people.inf.ethz.ch/omutlu/pub/ramulator_dram_simulator-ieee-cal15.pdf)  
[\[2\] Ghose et al. *Demystifying Complex Workload–DRAM Interactions: An Experimental Study.* SIGMETRICS 2019.](https://people.inf.ethz.ch/omutlu/pub/Workload-DRAM-Interaction-Analysis_sigmetrics19_pomacs19.pdf)  
[\[3\] Kim et al. *A Case for Exploiting Subarray-Level Parallelism (SALP) in
DRAM.* ISCA 2012.](https://users.ece.cmu.edu/~omutlu/pub/salp-dram_isca12.pdf)  
[\[4\] Lee et al. *Tiered-Latency DRAM: A Low Latency and Low Cost DRAM
Architecture.* HPCA 2013.](https://users.ece.cmu.edu/~omutlu/pub/tldram_hpca13.pdf)  
[\[5\] Seshadri et al. *RowClone: Fast and Energy-Efficient In-DRAM Bulk Data
Copy and Initialization.* MICRO
2013.](https://users.ece.cmu.edu/~omutlu/pub/rowclone_micro13.pdf)  
[\[6\] Chang et al. *Improving DRAM Performance by Parallelizing Refreshes with
Accesses.* HPCA 2014.](https://users.ece.cmu.edu/~omutlu/pub/dram-access-refresh-parallelization_hpca14.pdf)


## Usage

Ramulator supports four different usage modes.

1. **Memory Trace Driven:** Ramulator directly reads memory traces from a
  file, and simulates only the DRAM subsystem. Each line in the trace file 
  represents a memory request, with the hexadecimal address followed by 'R' 
  or 'W' for read or write.

  - 0x12345680 R
  - 0x4cbd56c0 W
  - ...


2. **CPU Trace Driven(Single-Threaded):** Ramulator directly reads instruction
  traces from a file, and simulates a simplified model of a "core" that
  generates memory requests to the DRAM subsystem. Each line in the trace file
  represents a memory request, and can have one of the following two formats.

  - `<num-cpuinst> <addr-read>`: For a line with two tokens, the first token 
        represents the number of CPU (i.e., non-memory) instructions before
        the memory request, and the second token is the decimal address of a
        *read*. 

  - `<num-cpuinst> <addr-read> <addr-writeback>`: For a line with three tokens,
        the third token is the decimal address of the *writeback* request, 
        which is the dirty cache-line eviction caused by the read request
        before it.

2. **CPU Trace Driven(Multi-Program):** Ramulator also supports multi-program simulation mode. It reads multiple trace files and instantiates the same number of cores to execute those traces simultaneously. We have a configurable cache model that supports several modes: only L1 and L2 cache, only last level cache(shared by all cores) and all three levels, so the contention of memory requests at the last level cache can be correctly modeled and we can also simulate with traces not filtered by any cache model. Our memory translation mechanism can map the same original address from different cores to different physical addresses. Random translation is supported now and can be enabled in the config file.

3. **gem5 Driven:** Ramulator runs as part of a full-system simulator (gem5
  \[7\]), from which it receives memory request as they are generated.

For some of the DRAM standards, Ramulator is also capable of reporting
power consumption by relying on either VAMPIRE \[8\] or DRAMPower \[9\] 
as the backend. 

[\[7\] The gem5 Simulator System.](http://www.gem5.org)  
[\[8\] Ghose et al. *What Your DRAM Power Models Are Not Telling You:
Lessons from a Detailed Experimental Study.* SIGMETRICS 2018.](https://github.com/CMU-SAFARI/VAMPIRE)  
[\[9\] Chandrasekar et al. *DRAMPower: Open-Source DRAM Power & Energy
Estimation Tool.* IEEE CAL 2015.](http://www.drampower.info)


## Getting Started

Ramulator requires a C++11 compiler (e.g., `clang++`, `g++-5`).

1. **Memory Trace Driven**

        $ cd ramulator
        $ make -j
        $ ./ramulator configs/DDR3-config.cfg --mode=dram dram.trace
        Simulation done. Statistics written to DDR3.stats
        # NOTE: dram.trace is a very short trace file provided only as an example.
        $ ./ramulator configs/DDR3-config.cfg --mode=dram --stats my_output.txt dram.trace
        Simulation done. Statistics written to my_output.txt
        # NOTE: optional --stats flag changes the statistics output filename

2. **CPU Trace Driven(Single-Threaded)**

        $ cd ramulator
        $ make -j
        $ ./ramulator configs/DDR3-config.cfg --mode=cpu cpu.trace
        Simulation done. Statistics written to DDR3.stats
        # NOTE: cpu.trace is a very short trace file provided only as an example.
        $ ./ramulator configs/DDR3-config.cfg --mode=cpu --stats my_output.txt cpu.trace
        Simulation done. Statistics written to my_output.txt
        # NOTE: optional --stats flag changes the statistics output filename

2. **CPU Trace Driven(Multi-Program)**

        $ cd ramulator
        $ make -j
        $ ./ramulator configs/DDR3-config.cfg --mode=cpu cpu.trace cpu.trace
        Simulation done. Statistics written to DDR3.stats
        # NOTE: Append all paths to trace files at the end of the command line, one for each core. The simulator will configure the core number accordingly.
        # NOTE: SPEC traces provided in folder `cputraces` have been filtered by a two level cache model, If you want to simulate a system with the last level cache, please change cache option to L3 in configuration file.
        $ ./ramulator configs/DDR3-config.cfg --mode=cpu --stats my_output.txt cpu.trace cpu.trace
        Simulation done. Statistics written to my_output.txt
        # NOTE: optional --stats flag changes the statistics output filename

3. **gem5 Driven**

   *Requires SWIG 2.0.12+, gperftools (`libgoogle-perftools-dev` package on Ubuntu)*

        $ hg clone http://repo.gem5.org/gem5-stable
        $ cd gem5-stable
        $ hg update -c 10231  # Revert to stable version from 5/31/2014 (10231:0e86fac7254c)
        $ patch -Np1 --ignore-whitespace < /path/to/ramulator/gem5-0e86fac7254c-ramulator.patch
        $ cd ext/ramulator
        $ mkdir Ramulator
        $ cp -r /path/to/ramulator/src Ramulator
        # Compile gem5
        # Run gem5 with `--mem-type=ramulator` and `--ramulator-config=configs/DDR3-config.cfg`

  By default, gem5 uses the atomic CPU and uses atomic memory accesses, i.e. a detailed memory model like ramulator is not really used. To actually run gem5 in timing mode, a CPU type need to be specified by command line parameter `--cpu-type`. e.g. `--cpu-type=timing`
        
## Simulation Output

Ramulator will report a series of statistics for every run, which are written
to a file.  We have provided a series of gem5-compatible statistics classes in
`Statistics.h`.

**Memory Trace/CPU Trace Driven**: When run in memory trace driven or CPU trace
driven mode, Ramulator will write these statistics to a file.  By default, the
filename will be `<standard_name>.stats` (e.g., `DDR3.stats`).  You can write
the statistics file to a different filename by adding `--stats <filename>` to
the command line after the `--mode` switch (see examples above).

**gem5 Driven**: Ramulator automatically integrates its statistics into gem5.
Ramulator's statistics are written directly into the gem5 statistic file, with
the prefix `ramulator.` added to each stat's name.

*NOTE: When creating your own stats objects, don't place them inside STL
containers that are automatically resized (e.g, vector).  Since these
containers copy on resize, you will end up with duplicate statistics printed
in the output file.*


## Reproducing Results from Paper (Kim et al. \[1\])


### Debugging & Verification (Section 4.1)

For debugging and verification purposes, Ramulator can print the trace of every
DRAM command it issues along with their address and timing information. To do
so, please turn on the `print_cmd_trace` variable in the configuration file.


### Comparison Against Other Simulators (Section 4.2)

For comparing Ramulator against other DRAM simulators, we provide a script that
automates the process: `test_ddr3.py`. Before you run this script, however, you
must specify the location of their executables and configuration files at
designated lines in the script's source code: 

* Ramulator
* DRAMSim2 (https://wiki.umd.edu/DRAMSim2): `test_ddr3.py` lines 39-40
* USIMM (http://www.cs.utah.edu/~rajeev/jwac12): `test_ddr3.py` lines 54-55
* DrSim (http://lph.ece.utexas.edu/public/Main/DrSim): `test_ddr3.py` lines 66-67
* NVMain (http://wiki.nvmain.org): `test_ddr3.py`  lines 78-79

Please refer to their respective websites to download, build, and set-up the
other simulators. The simulators must to be executed in saturation mode (always
filling up the request queues when possible).

All five simulators were configured using the same parameters:

* DDR3-1600K (11-11-11), 1 Channel, 1 Rank, 2Gb x8 chips
* FR-FCFS Scheduling
* Open-Row Policy
* 32/32 Entry Read/Write Queues
* High/Low Watermarks for Write Queue: 28/16

Finally, execute `test_ddr3.py <num-requests>` to start off the simulation.
Please make sure that there are no other active processes during simulation to
yield accurate measurements of memory usage and CPU time.


### Cross-Sectional Study of DRAM Standards (Section 4.3)

Please use the CPU traces (SPEC 2006) provided in the `cputraces` folder to run
CPU trace driven simulations.


## Other Tips

### Power Estimation

For estimating power consumption, Ramulator can record the trace of every DRAM
command it issues to a file in DRAMPower \[8\] format.  To do so, please turn
on the `record_cmd_trace` variable in the configuration file.  The resulting
DRAM command trace (e.g., `cmd-trace-chan-N-rank-M.cmdtrace`) should be fed
into a compatible DRAM energy simulator such as 
[VAMPIRE](https://github.com/CMU-SAFARI/VAMPIRE) \[8\] or 
[DRAMPower](http://www.drampower.info) \[9\] with the correct configuration 
(standard/speed/organization) to estimate energy/power usage for a single rank
(a current limitation of both VAMPIRE and DRAMPower).


### Contributors

- Yoongu Kim (Carnegie Mellon University)
- Weikun Yang (Peking University)
- Kevin Chang (Carnegie Mellon University)
- Donghyuk Lee (Carnegie Mellon University)
- Vivek Seshadri (Carnegie Mellon University)
- Saugata Ghose (Carnegie Mellon University)
- Tianshi Li (Carnegie Mellon University)
- @henryzh

### Acknowledgments

We thank the SAFARI group members who have contributed to
the initial development of Ramulator, including Kevin Chang, Saugata
Ghose, Donghyuk Lee, Tianshi Li, and Vivek Seshadri. We also
thank the anonymous reviewers for feedback. This work was
supported by NSF, SRC, and gifts from our industrial partners,
including Google, Intel, Microsoft, Nvidia, Samsung, Seagate
and VMware.
