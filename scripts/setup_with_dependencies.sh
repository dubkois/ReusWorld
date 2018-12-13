#!/bin/sh

project=(cxxopts json Tool AgnosticPhylogenicTree ReusWorld)
repos=(https://github.com/jarro2783/cxxopts.git https://github.com/nlohmann/json.git ssh://git@igit.odena.eu:5000/kevin/Tools.git ssh://git@igit.odena.eu:5000/kevin/AgnosticPhylogenicTree.git ssh://git@igit.odena.eu:5000/kevin/ReusWorld.git)

ppath=$(readlink ".local/")
mkdir -p $ppath
echo "Using $ppath as local installs root"

for i in "${!project[@]};"
do
  p=${project[$i]}
  r=${repos[$i]}
  
  echo "git clone $r"
  echo "cd $p"
  if grep -q 'igit.odena' <<< $r
  then
    echo "mkdir build_debug"
    echo "cd build_debug"
    echo "cmake .. -DCMAKE_PREFIX_PATH=$ppath -DWITH -DCMAKE_BUILD_TYPE=Debug"
  fi
done
