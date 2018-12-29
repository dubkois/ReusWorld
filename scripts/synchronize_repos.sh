#!/bin/bash

repos=(https://github.com/jarro2783/cxxopts.git https://github.com/nlohmann/json.git ssh://git@igit.odena.eu:5000/kevin/Tools.git ssh://git@igit.odena.eu:5000/kevin/AgnosticPhylogenicTree.git ssh://git@igit.odena.eu:5000/kevin/ReusWorld.git)

ppath=$(readlink "./local/")
mkdir -p $ppath
echo "Using $ppath as local installs root"

for repo in "${repos[@]}"
do
  echo "git clone $repo"
done

for repo in "${repos[@]};"
do
  if grep -q 'kevin' <<< ${repos[$i]}
  then
    echo "Setting up build folders"
    echo "mkdir build_debug"
    echo "cd build_debug"
    echo "cmake .. -DCMAKE_INSTALL_PREFIX=$ppath -DWITH -DCMAKE_BUILD_TYPE=Debug -DWITH_DEBUG_INFO=ON"
    echo "make -j5 && make install"
    echo "cd ../"
    echo "mkdir build_release"
    echo "cd build_release"
    echo "cmake .. -DCMAKE_INSTALL_PREFIX=$ppath -DWITH -DCMAKE_BUILD_TYPE=Release -DWITH_DEBUG_INFO=OFF"
    echo "cd ../../"
    echo "make -j5 && make install"
  fi
done
