#!/usr/bin/env python3

# This script tests different configurations of Ramulator using several traces.
# After making changes to the source code, run this script to make sure your changes do not break Ramulator
# in terms of simulation accuracy (i.e., cycles to complete a given trace), simulation speed, and system memory usage.

import subprocess
import sys, tempfile, psutil, time, random
import gzip, shutil
import colorama
from os import path

MEM_USAGE_THRESHOLD = 5 # Ramulator typically uses a few MiB (may depend on the compiler and compiler options). Give a warning if it exceeds this threshold
RUNTIME_ERROR_MARGIN = 0.2 # Prints a warning message if the simulation time exceeds the 'expected_runtime' of the 'traces'
                           # This is not an ideal way for checking the simulation performance since the runtime may differ a lot based on the machine Ramulator is running on
                           # It might be a good idea to edit the 'expected_runtime' of each trace in case of a mismatch in the runtime

configs = [ # EDIT
    './configs/DDR3-config.cfg',
    './configs/DDR4-config.cfg'
]

traces = [ # EDIT: when adding new traces, you can run the script with 0 'expected_runtime' and later modify it based on how long the simulation took
    {'trace_path': 'random-1m-0.8', 'expected_runtime': 15}, # random access DRAM trace with 1 millon requests and 80% READ requests
    {'trace_path': 'stream-1m-0.8', 'expected_runtime': 13}, # random access DRAM trace with 1 million requests and 80% READ requests
    {'trace_path': './cputraces/401.bzip2.gz', 'expected_runtime': 15},
    {'trace_path': './cputraces/403.gcc.gz', 'expected_runtime': 7},
]

SYNTHETIC_TRACE_TYPES = ['random', 'stream'] # DO NOT EDIT unless you are adding a new synthetic trace type

RAMULATOR_BIN = './ramulator' # EDIT if the Ramulator binary is in a different directory

def invalidTraceFormat(trace_name):
    print(f"ERROR: Invalid synthetic trace format: {trace_name}. Required format <{SYNTHETIC_TRACE_TYPES}>-<NUM_REQUESTS>-<READ_RATIO>. E.g., random-100k-0.8")
    sys.exit(-2)

def convert_str_to_number(x):
    num = 0
    num_map = {'K':1000, 'M':1000000, 'B':1000000000}
    if x.isdigit():
        num = int(x)
    else:
        if len(x) > 1:
            num = float(x[:-1]) * num_map.get(x[-1].upper(), 1)
    return int(num)

# generate the synthetic trace file file if it does not exist
def generateSyntheticTrace(trace_name):
    
    trace_options = trace_name.split('-')
    if len(trace_options) != 3:
        invalidTraceFormat(trace_name)
        
    trace_type = trace_options[0]

    if trace_type not in SYNTHETIC_TRACE_TYPES:
        invalidTraceFormat(trace_name)

    try:
        num_reqs = convert_str_to_number(trace_options[1])
        read_ratio = float(trace_options[2])
    except:
        invalidTraceFormat(trace_name)

    trace_filename = f'./{trace_type}_{trace_options[1]}_{trace_options[2]}.trace'

    if path.exists(trace_filename):
        return trace_filename # no need to regenerate the trace if it already exists

    print(f"Generating '{trace_type}' synthetic trace: {trace_filename} with {num_reqs} memory requests")

    trace_file = open(trace_filename, 'w')

    if trace_type == 'random':
        s = 64
        bits = 31

        l = int(s/64)
        b = int(num_reqs/l)

        for i in range(b):
            base = random.getrandbits(bits) & 0xffffffffffc0
            r = bool(random.random() < read_ratio)
            for j in range(l):
                trace_file.write('0x%x %s\n' % (base+j*64, 'R' if r else 'W'))
    
    if trace_type == 'stream':
        r = int(num_reqs * read_ratio)
        w = num_reqs - r
        for i in range(r):
            trace_file.write('0x%x %s\n' % (i*64, 'R'))
        for i in range(w):
            trace_file.write('0x%x %s\n' % ((r+i)*64, 'W'))

    trace_file.close()

    return trace_filename


def extractTrace(trace_path):
    
    uncompressed_path = trace_path.replace('.gz', '')

    if path.exists(uncompressed_path):
        return uncompressed_path # no need to extract if the .gz is already extracted

    print(f"Extracting {trace_path}")
    with gzip.open(trace_path, 'rb') as compressed_file:
        with open(uncompressed_path, 'wb') as uncompressed_file:
            shutil.copyfileobj(compressed_file, uncompressed_file)

    return uncompressed_path


def get_stat(stat_file, stat_name):
    stat_file.seek(0)
    for l in stat_file.readlines():
      if stat_name in l:
        return int(l.split()[1])


def isSyntheticTrace(trace):
    return any(tt in trace for tt in SYNTHETIC_TRACE_TYPES)

def compareAgainstGolden(stats_filename):

    STATS_ERROR_MARGIN = 0.1 # Allowing the stats to be up to 10% off wrt the golden stats
    stats_to_check = ['ramulator.dram_cycles'] # EDIT to check more statistics

    golden_stats_filename = stats_filename.replace('.stat', '.golden_stat')

    if not path.exists(golden_stats_filename):
        # save the current stat file as golden if no golden stat file exists
        print('Saving the current simulation result file {stats_filename} as a golden simulation result')
        shutil.copyfile(stats_filename, golden_stats_filename)
        return True

    stats_file = open(stats_filename, 'r')
    golden_stats_file = open(golden_stats_filename, 'r')

    mismatch = False

    for stat in stats_to_check:
        cur_val = get_stat(stats_file, stat)
        golden_val = get_stat(golden_stats_file, stat)

        if abs(cur_val - golden_val) > golden_val*(STATS_ERROR_MARGIN/2): # dividing by 2 since cur_val can be smaller or greater than the golden value
            print(f"WARNING: '{stat}' value is off by more than {int(STATS_ERROR_MARGIN*100)}% compared to the golden simulation results.")
            mismatch = True


    stats_file.close()
    golden_stats_file.close()


    return not mismatch



def main():
    blackhole = open('/dev/null', 'w')

    colorama.init()

    ok_str = colorama.Fore.GREEN + 'OK' + colorama.Style.RESET_ALL
    fail_str = colorama.Fore.RED + 'FAIL' + colorama.Style.RESET_ALL
    warn_str = colorama.Fore.YELLOW + 'WARNING:' + colorama.Style.RESET_ALL

    for trace in traces:
        trace_path = trace['trace_path']
        expected_trace_runtime = trace['expected_runtime']
        # Simulate each trace with each Ramulator config

        mode = '--mode=cpu'

        if isSyntheticTrace(trace_path):
            trace_path = generateSyntheticTrace(trace_path)
            mode = '--mode=dram' # synthetic traces are for --mode=dram
        
        if ".gz" in trace_path:
            trace_path = extractTrace(trace_path)

        for config in configs:

            trace_name = path.basename(trace_path).replace('.gz', '')
            dram_type = path.basename(config).replace('-config', '').replace('.cfg', '')
            stats_filename = f"{trace_name.replace('.trace', '')}_{dram_type}.stat"
            
            args = [RAMULATOR_BIN, config, mode, '--stats', stats_filename, trace_path]
            tmp = tempfile.NamedTemporaryFile()
            print(f"Starting simulation: {' '.join(args)}")
            p = subprocess.Popen(args, stdout=tmp.file, stderr=blackhole)
            proc = psutil.Process(p.pid)

            # monitor execution time and memory usage
            execution_time_sec, mem_usage_bytes = 0, 0
            while p.poll() is None:
                try:
                    mem_usage_bytes = max(mem_usage_bytes, proc.memory_info()[0])
                    execution_time_sec = sum(proc.cpu_times())
                except: print(f"======== Oops monitoring PID {p.pid} failed ===============")
                time.sleep(0.1)
            
            mem_usage_mib = float(mem_usage_bytes)/2**20

            mem_usage_ok = True
            if mem_usage_mib > MEM_USAGE_THRESHOLD:
                # If you see this warning, it is possible that your changes caused a memory leak or added data structures that made Ramulator use more memory
                print(f"{warn_str} Ramulator used {'{:.2f}'.format(mem_usage_mib)} MiB memory, which is more than the pre-defined threshold: {MEM_USAGE_THRESHOLD} MiB.")
                mem_usage_ok = False

            runtime_ok = True
            if execution_time_sec > expected_trace_runtime*(1 + RUNTIME_ERROR_MARGIN):
                print(f"{warn_str} Ramulator completed the simulation in {execution_time_sec} seconds, which is more than {int(RUNTIME_ERROR_MARGIN*100)}% higher than the expected runtime: {expected_trace_runtime} seconds.")
                runtime_ok = False

            stats_ok = compareAgainstGolden(stats_filename)

            print(f"Stat Consistency: {ok_str if stats_ok else fail_str}, Runtime: {ok_str if runtime_ok else fail_str}, Memory Usage: {ok_str if mem_usage_ok else fail_str}")

    blackhole.close()


if __name__ == '__main__':
    main()