#!/bin/bash

now=$(LC_ALL=C date | tr " :" "_-")
log="reposync_log_$now"
echo "## $(LC_ALL=C date) ##" | tee $log
echo "Synchronizing repositiories" | tee -a $log

ppath=$(readlink -m "./local/")
mkdir -p $ppath
echo "Using '$ppath' as local installs root" | tee -a $log
cmakeopts="-DCMAKE_INSTALL_PREFIX=$ppath -DCMAKE_BUILD_TYPE=Release -DWITH_DEBUG_INFO=OFF"
makeopts="-j5"
makeinstall="$makeopts install"

#Name;URL;Version;CMake options;Make options;
data="\
cxxopts;https://github.com/jarro2783/cxxopts.git;v2.1.1;-;-
json;https://github.com/nlohmann/json.git;v3.5.0;-;-
Tools;ssh://git@igit.odena.eu:5000/kevin/Tools.git;-;$cmakeopts;$makeinstall
AgnosticPhylogenicTree;ssh://git@igit.odena.eu:5000/kevin/AgnosticPhylogenicTree.git;-;$cmakeopts;$makeinstall
ReusWorld;ssh://git@igit.odena.eu:5000/kevin/ReusWorld.git;-;$cmakeopts;$makeopts"

echo
wd=$(pwd)
while IFS=';' read name url version cmake make
do
  echo "Processing $name"
  
  if [ ! -d "$name" ]
  then
    git clone $url
  fi

  cd $name
  
  if [ "$version" != "-" ]
  then
    git fetch --tags
    git checkout $version --detach
  else
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
