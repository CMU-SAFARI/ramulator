#!/bin/sh 
#Usage: Step2.sh DataDir [warmup-factor] [cutoff]
# arg1: Name of the directory for SimPoint processing.
# Optional: arg2 : warmup-factor -- number of slices *before* a simulation
#  point. Intended to be used as a warmup region. A PinPoint will be
#  emitted for the first slice of the warmup region instead of the actual
#  simulation region.
# Optional: arg3 : cutoff 0 < X < 1.0
#   The fraction of execution to be represented by Simulation Points.
#   The default is .9999 implying Simulation points will represent 100% of
#   the execution.
# Last two rguments are optional but to use 'cutoff' you have to to 
# use 'warmup-factor'.
#
# Optional: MAXK : An environment variable to denote the  maximum number of 
#   simulation regions to select. If not set, a value of 10 will be used.

# Exit on error
ERROR ( )
{
    echo "  *** Problem running Step2 of pinpoints kit ***"
    echo "  *** See FAQ  in ./README ***"
    echo ""
    exit
}

GET_YES_NO ( )
{
    echo "  'yes' to continue, 'no' to exit."
    read ACCEPTANCE &> /dev/null
    if [ "$ACCEPTANCE" = no ] ; then
    ERROR
    elif [ "$ACCEPTANCE" != yes ] ; then
    GET_YES_NO
    fi
}

WARMUP=0
CUTOFF=0.9999
if [ $# -eq 0 ]
then
    echo "#Usage: Step2.sh DataDir [warmup-factor] [cutoff]"
    ERROR
fi

if [ -z $MAXK ]
then
    echo "Environment variable MAXK not set, using 10 "
    MAXK=10
fi

SIMDIR=$1
if [ $# -eq 2 ]
then
    WARMUP=$2
    echo 'WARMUP FACTOR' = $WARMUP
fi
if [ $# -eq 3 ]
then
    WARMUP=$2
    echo 'WARMUP FACTOR' = $WARMUP
    CUTOFF=$3
    echo 'CUTOFF' = $CUTOFF
fi

# If someone uses .9 change it to 0.9
CUTOFF=`echo $CUTOFF | sed '/^\./s//0./g'`

# Filter trailing zeros
CUTOFF=`echo $CUTOFF | sed '/00*$/s//0/g'`

echo 'Filtered CUTOFF' = $CUTOFF

if [ $CUTOFF == "1.0" ]
then
    CUTOFF_SUFFIX=""
else
    CUTOFF_SUFFIX=".lpt$CUTOFF"
fi
echo 'CUTOFF_SUFFIX' = $CUTOFF_SUFFIX

echo "Welcome to Step 2 : Generating pinpoints file"
echo "This step is almost completely automated."
if which simpoint"$SIMPOINT_SUFFIX" 
then
    echo "PATH set correctly."
else
    echo "Please add simpoint"$SIMPOINT_SUFFIX"  tools to `pwd`/bin."
    echo "Add `pwd`/bin to your PATH"
    exit
fi


mkdir -p $SIMDIR
cd $SIMDIR

echo " Name the profile file generated in Step1:"
read TMP &> /dev/null

for i in $TMP
do
if  [ ! -r $i ];  then
    echo "`pwd`/$TMP does not exist or is not readable."
    ERROR
fi
done

extension=${TMP##*.}
if   [ $extension != "bb" ]; then
    echo "`pwd`/$TMP is not a bb file."
    ERROR
fi


if  cat $TMP > t.bb 
then
    echo "Concatenated $TMP"
else
    echo "Error concatenating $TMP"
    ERROR
fi

#let inscount=`grep Dynamic t.bb | tail -1 | awk '{print $NF}'`
#let slicesize=`cat t.bb | grep SliceSize | tail -1 | awk '{print $NF}'`
# 'let' command does not work on older (7.1) Red Hat versions
# in that case coment the two lines above and uncomment the two lines
# below
inscount=`grep Dynamic t.bb | awk '{print $NF}' | sort -n |  tail -1` 
slicesize=`cat t.bb | grep SliceSize | tail -1 | awk '{print $NF}'`

echo "slicesize=$slicesize"
echo "inscount=$inscount"

if  [ $slicesize -gt $inscount ] ; then
    echo "SliceSize ($slicesize) is too large (> total # instructions: $inscount)"
    tmp=`echo "($inscount/100)" | bc`
    recommended_slice_size=`echo "10^(length($tmp)-1)" | bc`
    echo "Recommended SliceSize == $recommended_slice_size"
    echo "Please re-run Step1 with 'pin -t `which isimpoint` -slice_size $recommended_slice_size --'"
    exit
fi

simpoint"$SIMPOINT_SUFFIX"  -loadFVFile ./t.bb -maxK $MAXK -coveragePct $CUTOFF -saveSimpoints ./t.simpoints -saveSimpointWeights ./t.weights -saveLabels t.labels > simpoint.out

if  [ -e  "t.simpoints" ];  then
    echo "Ran UCSD Cluster Analysis Successfully"
else
    echo "Error running UCSD Cluster Analysis."
    ERROR
fi

echo
echo "Creating pinpoints file : version1:"
echo "      What is the prefix for the file (short name of your program)?"
read TMP &> /dev/null


echo "Creating pinpoints file for PIN tools:"
echo  "ppgen.3 $TMP.pintool t.bb t.simpoints$CUTOFF_SUFFIX t.weights$CUTOFF_SUFFIX t.labels $WARMUP >& ppgen.out"

ppgen.3 $TMP.pintool t.bb t.simpoints$CUTOFF_SUFFIX t.weights$CUTOFF_SUFFIX t.labels $WARMUP >& ppgen.out

if  [ -e  "$TMP.pintool.1.pp" ];  then
    echo ""
    echo "Generated pinpoints file `pwd`/$TMP.pintool.1.pp."
else
    echo "Error generating PIN tool pinpoints file."
    ERROR
fi


list="t.simpoints$CUTOFF_SUFFIX "
for i in 2 3 4
do
    echo "Creating pinpoints file #$i for PIN tools:"
    echo  "pick_alternate_simpoints.sh $i t.simpoints$CUTOFF_SUFFIX t.labels > t.$i.simpoints$CUTOFF_SUFFIX"
    pick_alternate_simpoints.sh $i t.simpoints$CUTOFF_SUFFIX t.labels > t.$i.simpoints$CUTOFF_SUFFIX
    echo "ppgen.3 $TMP.ALT$i.pintool t.bb t.$i.simpoints$CUTOFF_SUFFIX t.weights$CUTOFF_SUFFIX t.labels $WARMUP >& ppgen.out"
    ppgen.3 $TMP.ALT$i.pintool t.bb t.$i.simpoints$CUTOFF_SUFFIX t.weights$CUTOFF_SUFFIX t.labels $WARMUP >& ppgen.out
    if  [ -e  "$TMP.ALT$i.pintool.1.pp" ];  then
        echo ""
        echo "Generated alternate pinpoints file `pwd`/$TMP.ALT$i.pintool.1.pp."
    else
        echo "Error generating alternate PIN tool pinpoints file."
        ERROR
    fi
    list=${list}" ""t.$i.simpoints$CUTOFF_SUFFIX"
done
echo "Step2:  Done "
