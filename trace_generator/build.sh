#!/bin/bash
echo "$(whoami)"
[ "$UID" -eq 0 ] || exec sudo -E "$0" "$@"
if [ -z "${PIN_ROOT}" ]; then
  echo "set PIN_ROOT."
  exit 1
fi
cd Simpoint3.2
make Simpoint
cd ..
cd src/PinPoints
echo -e "${RED}make isimpoint.test${DEF}"
make isimpoint.test
cd ..
echo -e "${RED} make gettrace.test${DEF}"
make gettrace.test
sudo rm -rf trace.out 