#!/bin/sh

# Exit on error
ERROR ( )
{
    echo "  *** Problem running Step.branch_predictor of pinpoints kit ***"
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

echo "Welcome to : Running a simple branch-predictor with pinpoints."
echo "This step is NOT fully automated."
if which branch_predictor
then
    echo "PATH set correctly."
else
    echo "Add `pwd`/bin to your PATH"
    exit
fi
echo " Find out how the IPF/Linux binary for your program is invoked. "
echo "(Separate window) Run the program by prefixing it with '`which branch_predictor` -ppfile PathToPintoolPinpointsFile '"
echo "  e.g. instead of 'hello' invoke your program as "
echo "      '`which branch_predictor` -ppfile /proj/johndoe/hello.pintool.1.pp -- hello'"
echo " Did your program run with '`which branch_predictor` '?"
GET_YES_NO

echo "Locate the file "bimodal.out"  where you ran your program."
echo "  Where is your bimodal.out file?"
echo "    Specify path starting with '/'" 
read TMP &> /dev/null

if [ "$TMP" != "" ]; then
for i in $TMP
do
    if [ ! -r $i  ]; then
        echo "  $i  does not exist!"
        ERROR
    fi
done
fi

cat $TMP
echo "Step.branch_predictor:  Done "
