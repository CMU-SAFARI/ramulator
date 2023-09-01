#!/bin/sh

# Exit on error
ERROR ( )
{
    echo "  *** Problem running Step1 of pinpoints kit ***"
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

echo "Welcome to Step 1 : profiling your program."
echo "This step is NOT fully automated."
if which isimpoint
then
    echo "PATH set correctly."
else
    echo "Add `pwd`/bin to your PATH"
    exit
fi
echo " Find out how the x86/Linux binary for your program is invoked. "
echo "(Separate window) Run the program by prefixing it with 'pin -t `which isimpoint` --'"
echo "  e.g. instead of "hello" invoke your program as ' pin -t `which isimpoint` -o hello.bb hello'"
echo " Did your program run with 'pin -t `which isimpoint` '?"
GET_YES_NO

echo "Locate the file of the form *.bb  where you ran your program."
echo "  Where is your *.bb file?"
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
if   [ $extension != "bb" ]; then
    echo "`pwd`/$TMP does not have the extension 'bb'."
    ERROR
fi  

echo "cp $TMP `pwd`/Data"
cp $TMP `pwd`/Data
echo "Step1:  Done "
