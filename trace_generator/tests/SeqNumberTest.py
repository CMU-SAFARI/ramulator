import sys
oldno=-1
old_write=False
lineno=0
error=0
if(len(sys.argv)<3):
    print "This test checks if the sequence numbers are correctly recorded."
    print "USAGE: python SeqNumberTest.py <dependency trace file> <window size : if not changed give 128>"
    exit(1);
for line in open(sys.argv[1]):
    tokens = line.split(" ")
    if(tokens[0]=="#"):
        lineno += 1
        continue
    if((oldno == int(sys.argv[2])-1) and tokens[0] != "0"):
        if(old_write and tokens[1] == "READ"):
            lineno += 1
            old_write=False
            continue
        print "Error in line no : " + str(lineno) + " expected : 0" + " found : " + tokens[0]
        error += 1
    if(oldno != int(sys.argv[2])-1 and (oldno + 1) != int(tokens[0]) ):
        if(old_write and tokens[1] == "READ"):
            lineno += 1
            old_write=False
            continue
        print "Error in line no : " + str(lineno) + " expected : " + str(oldno+1) + " found : " + tokens[0]
        error += 1
    #print  "seq no : " + tokens[0]
    lineno = lineno + 1
    if(oldno != int(sys.argv[2])-1):
        oldno += 1
    else:
        oldno = 0
    if(tokens[1] == "WRITE"):
        old_write=True
    else:
        old_write=False
if(error>0):
    print "FAILED error: " + str(error)
else:
    print "SUCCESS"
