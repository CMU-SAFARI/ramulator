#!/bin/sh

# Exit on error
ERROR ( )
{
    echo "  *** Problem running Step.check_pinpoints of pinpoints kit ***"
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

echo "Welcome to : checking pinpoints for your program."
echo "This step is NOT fully automated."
if which controller
then
    echo "PATH set correctly."
else
    echo "Add `pwd`/bin to your PATH"
    exit
fi
echo " Find out how the x86/Linux binary for your program is invoked. "
echo "(Separate window) Run the program by prefixing it with 'pin `which controller` -control_log control.log -ppfile PathToPintoolPinpointsFile '"
echo "  e.g. instead of 'hello' invoke your program as "
echo "      'pin -t `which controller` -control_log control.log -ppfile /proj/johndoe/hello.pintool.1.pp -- hello'"
echo " Did your program run with 'pin -t `which controller` '?"
GET_YES_NO

echo "Locate the file "control.log"  where you ran your program."
echo "  Where is your control.log file?"
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

extension=${TMP##*.}
if   [ $extension != "log" ]; then
    echo "`pwd`/$TMP does not have the extension 'log'."
    ERROR
fi  

echo "cp $TMP `pwd`/Data"
cp $TMP `pwd`/Data
report_controller_output.sh `pwd`/Data/control.log
echo "Step.check_pinpoints:  Done "
