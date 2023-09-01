#!/bin/bash
echo "$(whoami)"
if [ "$UID" -ne 0 ]; then
  echo -e "[INFO] This tool needs root permissions. If ASLR is disabled in your system and you are generating traces with virtual addresses you can use \"-test\" option."
fi
[ "$UID" -eq 0 ] || exec sudo -E "$0" "$@"

if [ -z "${PIN_ROOT}" ]; then
  echo "Set PIN_ROOT."
  exit 1
fi
starting=`pwd`
pin_args=""
prgname=""
coverage=""
cvgfextension=""
filename=""
outfile=""
force=false
test=false
sp=false
isize=100000000
ipass=false
cachefile=""
while [ -n "$1" ]; do #parsing the command line arguments
    if [[ "$ipass" = false ]]; then
      case "$1" in
      -force)
        force=true;;
      -test)
        test=true;;
      -intervalsize)
        isize="$2"
        ipass=true;;
      -fast)
        sp=true
        pin_args="$pin_args -fast 1";;
      --) prgname="${@:2}"
        break;;
      -cvg) sp=true
  	    coverage="$2"
        pin_args="$pin_args -cvg ";;
      -t) outfile="$2"
        pin_args="$pin_args -t ";;
      -c) cachefile="$2"
        pin_args="$pin_args -c ";;
      on) pin_args="$pin_args 1 ";;
      off) pin_args="$pin_args 0 ";;
      *) pin_args="$pin_args $1" ;; # real pin arguments
      esac
    else
      ipass=false
    fi
    shift
done
echo $prgname
#echo "args:$pin_args"
if [[ ! -z "$coverage" ]]; then
  cvgfextension=".lpt0$coverage"
  coverage="-coveragePct $coverage"
fi
if [ -z "$prgname" ]; then
  echo "USAGE: ./tracegenerator.sh -- <programname>"
  # ${PIN_ROOT}/pin -t obj-intel64/gettrace.so -help
  exit 1
fi
if [ -z "$outfile" ]; then
  filename="tg$$"
  # filename=$(echo $prgname | tr -d . | tr -d ' ' | tr -d /)
else
  filename=$outfile
fi
if [ -z "$cachefile" ]; then
  cachefile="src/Cache.cfg"
fi
if [ "$force" = true ]; then
  echo "[INFO] force compile"
  cd src
  make clean
  cd PinPoints
  make clean
  cd ../../
fi
RED='\033[1;31m'
GRE='\033[0;32m'
DEF='\033[0m'
STARTTIME=$(date +%s)
directory="temp-$filename"
mkdir $directory

if [ "$test" = false ]; then
  # disabling ASLR
  sudo sysctl -w kernel.randomize_va_space=0
fi

if [ "$sp" = true ]; then
  if [ "$force" = true ]; then
    # Generating frequency vector file
    cd src/PinPoints
    # echo -e "${RED}make isimpoint.test${DEF}"
    make isimpoint.test
    cd $starting
  fi
  echo -e "${RED}${PIN_ROOT}/pin -t src/PinPoints/obj-intel64/isimpoint.so -slice_size $isize -o $directory/$filename -- $prgname${DEF}"
  ${PIN_ROOT}/pin -t src/PinPoints/obj-intel64/isimpoint.so -slice_size $isize -o $directory/$filename -- $prgname

  # Generating simulation points
  # echo -e "${RED}Simpoint3.2/bin/simpoint -maxK 30 -saveSimpoints $directory/$filename.pts -saveSimpointWeights $directory/$filename.w $coverage -saveLabels $directory/$filename.lbl -loadFVFile $directory/$filename.T.0.bb > $directory/$filename.simpoint.log${DEF}"
  Simpoint3.2/bin/simpoint -maxK 30 -saveSimpoints $directory/$filename.pts -saveSimpointWeights $directory/$filename.w $coverage -saveLabels $directory/$filename.lbl -loadFVFile $directory/$filename.T.0.bb > $directory/$filename.simpoint.log
  # echo "Simpoint log can be found at $directory/$filename.simpoint.log"

  # Getting simpoints execution count and PC values
  # echo -e "${RED}perl src/PinPoints/bin/ppgen.3 $directory/$filename $directory/$filename.T.0.bb $directory/$filename.pts$cvgfextension $directory/$filename.w$cvgfextension $directory/$filename.lbl 0${DEF}"
  perl src/PinPoints/bin/ppgen.3 $directory/$filename $directory/$filename.T.0.bb $directory/$filename.pts$cvgfextension $directory/$filename.w$cvgfextension $directory/$filename.lbl 0
fi

if [ "$force" = true ]; then
  # compiling the trace generator tool
  cd src
  # echo -e "${RED} make gettrace.test${DEF}"
  make gettrace.test
  rm -rf trace.out
  cd $starting
fi

# collecting traces
echo -e "${PIN_ROOT}/pin -t src/obj-intel64/gettrace.so -c $cachefile -intervalsize $isize  $pin_args -ppoints $directory/$filename.1.pp  -- $prgname"
${PIN_ROOT}/pin -t src/obj-intel64/gettrace.so -intervalsize $isize $pin_args -c $cachefile -ppoints $directory/$filename.1.pp  -- $prgname # TODO: config file needs to change.


ENDTIME=$(date +%s)
echo -e "Trace Generation Time (seconds)\t\t\t\t:$(($ENDTIME - $STARTTIME))"
echo -e "${GRE}Temporary files are under $directory directory.${DEF}"

outputname=""
if [ -z $outfile ]; then
  outputname="trace.out"
else
  outputname="$outfile"
fi

if [ "$sp" = true ]; then
  outputname="$outputname.*"
fi
for filename in ./$outputname; do
  size="$(wc -c <"$filename")"
  if [ "$size" -eq 0 ]; then
    #Warning
    echo -e "${RED}Trace generator couldn't record enough memory requests for $filename. You can consider changing the cache configurations or the interval size (using '-intervalsize').  ${DEF}"
  fi
done

if [ "$test" = false ]; then
  # enabling ASLR
  sudo sysctl -w kernel.randomize_va_space=2
fi
