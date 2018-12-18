#!/bin/bash

now=$(LC_ALL=C date | tr " :" "_-")
log="setup_log_$now"
echo "## $(LC_ALL=C date) ##" | tee $log
echo "Setting up build system" | tee -a $log

ppath=$(readlink -m "./local/")
mkdir -p $ppath
echo "Using '$ppath' as local installs root" | tee -a $log
cmakeopts="-DCMAKE_INSTALL_PREFIX=$ppath -DCMAKE_BUILD_TYPE=Release -DWITH_DEBUG_INFO=OFF"
makeopts="-j5"
makeinstall="$makeopts install"

data="\
cxxopts;https://github.com/jarro2783/cxxopts.git;-;-
json;https://github.com/nlohmann/json.git;-;-
Tools;ssh://git@igit.odena.eu:5000/kevin/Tools.git;$cmakeopts;$makeinstall
AgnosticPhylogenicTree;ssh://git@igit.odena.eu:5000/kevin/AgnosticPhylogenicTree.git;$cmakeopts;$makeinstall
ReusWorld;ssh://git@igit.odena.eu:5000/kevin/ReusWorld.git;$cmakeopts;$makeopts"

wd=$(pwd)
while IFS=';' read name url cmake make
do
  echo "Processing $name"
  if [ ! -d "$name" ]
  then
    git clone $url
    cd $name
  else
    cd $name
    git pull
  fi
  if [ "$cmake" != "-" ]
  then
    if [ ! -d "build" ]
    then
      echo ">> Setting up release build folder"
      mkdir build
      cd build
      cmake .. $cmake
    else
      cd build
    fi
    make $make
  fi
  cd $wd
  printf "Done.\n\n"

done <<< "$data" | tee -a $log

printf "All done\n" | tee -a $log
