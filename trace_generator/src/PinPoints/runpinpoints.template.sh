#!/bin/bash
# This is a template that combines Step1.sh, Step2.sh, and  
# Step.check_pinpoints.sh from  the pinpoints kit.
# The goal is to [1] profile the workload, [2] find representative PinPoints,
# [3] check that the PinPoints can be reached.
#
# See the lines tagged with CHANGME below

echo "***** PinPoints: START  ***** `date`"

#Customizeable parameters : CHANGE as/if needed
export SLICE_SIZE=3000000 #CHANGME(if needed) instruction count for each region traced
export MAXK=20 #CHANGME(if needed) Maximumu number of Phases/PinPoints
export CUTOFF=".99" #CHANGME(if needed) find regions representing 99% of the execution
                   # useful for discarding low-weight regions
                   # use a value 0 < CUTOFF <=1.0
export WARMUP_FACTOR=0 #CHANGME(if needed) Number of regions of size SLICE_SIZE to be used for warmup (CAT file length will be reduced b PP_PRE_PAD_FACTOR)
#environment variables
export PINHOME=/tmp_proj/harish/pin-2.2-14297-gcc.4.0.0-ia32e-linux/PinPoints
export PATH=$PINHOME/bin:$PATH # to get isimpoint, simpoint, and controller.
export PATH=$PINHOME/../Bin:$PATH # to get the 'pin' binary
export PINFLAGS=""
export PINTOOLFLAGS=""

#Prefix specific to the workload : TO BE CHANGED 
# No CAPITAL letters and no periods (".") please.
export PROGRAM="eon" # CHANGEME
export INPUT="rushmeier" # CHANGEME
export WORKDIR=`pwd`
export COMMAND="base.exe Dat/chair.control.rushmeier Dat/chair.camera Dat/chair.surfaces dummy.ppm ppm pixels_out.rushmeier > /dev/null 2>&1" # CHANGEME Do not redirect stderr if you want to see error messages from Pin

export PINFILEPREFIX=$PROGRAM.$INPUT

echo "PROGRAM.INPUT $PROGRAM.$INPUT"
echo "SLICE_SIZE=$SLICE_SIZE"
echo "CUTOFF=$CUTOFF"

#Prefix not related to the workload: DO NOT Change

#CHANGEME
#If you would like your own way of naming the tracefiles set TRACEFILE above
# as desired.

startdir=`pwd`
#pin starts from here
bbname=$PINFILEPREFIX
if [ ! -e $PINHOME ] 
then
	echo "$PINHOME is missing"
	exit
fi
if [ ! -e $WORKDIR ] 
then
	echo "$WORKDIR is missing"
	exit
fi

cd $WORKDIR
echo "#!/bin/sh" > $PINFILEPREFIX.do.it.sh
echo "#Automatically generated; do not modify." >> $PINFILEPREFIX.do.it.sh
echo "\$PIN_PREFIX $COMMAND" >> $PINFILEPREFIX.do.it.sh
chmod +x $PINFILEPREFIX.do.it.sh

# BEGIN : FOR ALT PINPOINT-FILE RUN START COMMENTING FROM HERE
echo "Running $COMMAND in $WORKDIR"
echo  "PIN_PREFIX=$PIN_PREFIX"
eval "time ./$PINFILEPREFIX.do.it.sh" >& t.$$
cat t.$$ | grep "^user" > $PINFILEPREFIX.NATIVE.TIME
cat t.$$; rm -f t.$$

echo "*** Running isimpoint *** `date`"
cd $WORKDIR
export PIN_PREFIX=" pin $PINFLAGS -t `which isimpoint` $PINTOOLFLAGS -slice_size $SLICE_SIZE -o $bbname -- " 
echo  "PIN_PREFIX=$PIN_PREFIX"
time ./$PINFILEPREFIX.do.it.sh 
bbfile=`ls *.bb`
mkdir -p $PINFILEPREFIX.Data
if [ -e $bbname.T.0.bb ] 
then
	cp $bbname.T.0.bb $PINFILEPREFIX.Data/
else
	echo "$bbname.T.0.bb is missing"
	exit
fi


echo "*** Running Step2.sh *** `date`"
$PINHOME/Step2.sh $PINFILEPREFIX.Data $WARMUP_FACTOR $CUTOFF<< END
$bbname.T.0.bb
$bbname
END
# END : FOR ALT PINPOINT-FILE RUN END COMMENTING HERE


# FOR ALT PINPOINT-FILE RUN CHANGE THE FOLLOWING LINE
pintool1ppfile=$PINFILEPREFIX.pintool.1.pp
echo "*** Changing dir to $WORKDIR ***"

cp $PINFILEPREFIX.Data/*.pp .

echo "*** Running controller *** `date`"
export PIN_PREFIX=" pin $PINFLAGS -t `which controller` $PINTOOLFLAGS -ppfile $pintool1ppfile -control_log $bbname.control.log -- "
echo  PIN_PREFIX=$PIN_PREFIX
time ./$PINFILEPREFIX.do.it.sh 

echo "*** Checking controller output*** `date`"
cat $bbname.control.log | grep "Starting"  > t.t
maxpp=`cat $bbname.control.log  | grep Starting | wc -l`
let ppno=1
echo "Pinpoint, Start, Actual_Start, %delta_start, Length, Actual_length, %delta_start"
while [ $ppno -le $maxpp ]; do
     echo -n "$ppno, "
     ppstart=`cat t.t | head -$ppno | tail -1 | awk '{print $NF}'`
     pplength=`cat t.t | head -$ppno | tail -1 | sed '/.* length /s///g' | awk '{print $1}'`
     actstart=`cat t.t | head -$ppno | tail -1 | sed '/.* Actual_start /s///g' | awk '{print $1}'`
     actlength=`grep Stopping $bbname.control.log | head -$ppno | tail -1 | awk '{print $NF}'`
    if [ $(( $ppstart == 0 )) = 1 ] ; then
        sdelta=0
    else
        sdelta=`echo "scale=3; 100*($actstart-$ppstart)/$ppstart" | bc`
    fi
    ldelta=`echo "scale=3; 100*($actlength-$pplength)/$pplength" | bc`
    echo "$ppstart, $actstart, $sdelta, $pplength, $actlength, $ldelta" 
     let ppno=ppno+1
done


echo "***** PinPoints: END  ***** `date`"
cd $startdir
