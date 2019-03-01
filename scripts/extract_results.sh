#!/bin/bash

if [ $# -ne 2 ]
then
  echo "Usage: $0 <build-folder> <result-folder>"
  echo "       Uses executables in <build-folder> to generate results into <result-folder>"
  exit 10
fi

buildFolder=$(readlink -f build_$1)
if [ ! -d "$buildFolder" ]
then
  echo "ERROR: build folder '$1' does not exist. Aborting"
  exit 1
fi

resultFolder=$(readlink -f $2)
if [ ! -d "$resultFolder" ]
then
  echo "ERROR: result folder '$2' does not exist. Aborting"
  exit 2
fi

outFolder=$resultFolder/sumup

mkdir -vp $outFolder $outFolder/morphologies

if [ -z "$(ls -A $outFolder/morphologies/)" ]
then
  $buildFolder/visualizer --load=$resultFolder/autosaves/last_run_y100.save.ubjson --collect-morphologies=$outFolder/morphologies/
else
  echo "Morphologies folder already contains stuff"
fi

if [ ! -f $outFolder/phylogeny.png ]
then
  $buildFolder/pviewer --config=$resultFolder/configs/PTree.config --tree=$resultFolder/phylogeny.ptree.json --print $outFolder/phylogeny.png
else
  echo "Phylogenic tree already generated"
fi

if [ ! -f $outFolder/dynamics_time.png ]
then
  $buildFolder/../scripts/plant_dynamics.sh -f $resultFolder/global.dat -c "Plants;Time:y2" -o $outFolder/dynamics_time.png
else
  echo "Time dynamics already generated"
fi

if [ ! -f $outFolder/dynamics_species.png ]
then
  $buildFolder/../scripts/plant_dynamics.sh -f $resultFolder/global.dat -c "Plants;Species:y2" -o $outFolder/dynamics_species.png
else
  echo "Species dynamics already generated"
fi
