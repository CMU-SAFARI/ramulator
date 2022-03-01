import sys
import os
import subprocess
if(len(sys.argv)<2):
    print "This test checks if the bubble counts in two traces generated with the same input are the same."
    print "USAGE: sudo -E python TraceTest.py <program name>"
    exit(1);
prgname = sys.argv[1]
test1 = './tracegenerator.sh -icache off -paddr off -t test1 -mode datadep -test -- ' + os.getcwd() + "/" + prgname
test2 = './tracegenerator.sh -icache off -paddr off -t test2 -mode datadep -test -- ' + os.getcwd() + "/" + prgname
os.chdir("../")
subprocess.call('sysctl -w kernel.randomize_va_space=0',shell=True)
subprocess.call(test1,shell=True)
subprocess.call(test2,shell=True)
subprocess.call('sysctl -w kernel.randomize_va_space=2',shell=True)
first="test1"
second="test2"
linecount=0
difcount=0
with open(first, "r") as f, open(second,"r") as s:
    for line, line2 in zip(f,s):
        if(line.split()[0]!=line2.split()[0]):
            difcount=difcount+1
            print "diff at: " + str(linecount)
        linecount=linecount+1
print "total diff:" + str(difcount)
print "total line:" + str(linecount)
if(difcount==0):
    print "SUCCESS"
else:
    print "FAILED"
