#!/bin/sh
#Usage : report_controller_output.sh log-file
ERROR()
{
  echo "Usage : report_controller_output.sh log-file"
  exit
}
if  [ $# != 1 ];  then
    echo "Not enough arguments!"
    ERROR
fi
cat $1 | grep "Starting"  > t.t
maxpp=`cat $1  | grep Starting | wc -l`
let ppno=1
echo "Pinpoint, Start, Actual_Start, %delta_start, Length, Actual_length, %delta_start"
while [ $ppno -le $maxpp ]; do
     echo -n "$ppno, "
     ppstart=`cat t.t | head -$ppno | tail -1 | awk '{print $NF}'`
     pplength=`cat t.t | head -$ppno | tail -1 | sed '/.* length /s///g' | awk '{print $1}'`
     actstart=`cat t.t | head -$ppno | tail -1 | sed '/.* Actual_start /s///g' | awk '{print $1}'`
     actlength=`grep Stopping $1 | head -$ppno | tail -1 | awk '{print $NF}'`
    if [ $(( $ppstart == 0 )) = 1 ] ; then
        sdelta=0
    else
        sdelta=`echo "scale=3; 100*($actstart-$ppstart)/$ppstart" | bc`
    fi
    ldelta=`echo "scale=3; 100*($actlength-$pplength)/$pplength" | bc`
    echo "$ppstart, $actstart, $sdelta, $pplength, $actlength, $ldelta" 
     let ppno=ppno+1
done
