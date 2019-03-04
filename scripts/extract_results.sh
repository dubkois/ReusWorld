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

if [ ! -f $resultFolder/phylogeny.ptree.json ]
then
  echo "No phylogeny.ptree.json file in '$resultFolder'. Presumably not completed. Skipping"
  
else
  echo "Generating content into '$outFolder'"
  mkdir -vp $outFolder $outFolder/morphologies

  morphoOut=$outFolder/morphologies/
  if [ -z "$(ls -A $outFolder/morphologies/)" ]
  then
    lastSave=$(find $resultFolder/autosaves/ -name "*.save.ubjson" -printf "%T@ %p\n" | sort -n | cut -d " " -f 2- | tail -n 1)
    $buildFolder/visualizer --verbosity=QUIET --load=$lastSave --collect-morphologies=$morphoOut
  else
    echo "Morphologies folder '$morphoOut' already contains stuff"
  fi

  ptreeOut=$outFolder/phylogeny.svg
  if [ ! -f $ptreeOut ]
  then
    $buildFolder/pviewer --verbosity=QUIET --config=$resultFolder/configs/PTree.config --tree=$resultFolder/phylogeny.ptree.json --minEnveloppe 1 --showNames=false --print $ptreeOut
  else
    echo "Phylogenic tree '$ptreeOut' already generated"
  fi

  dtOut=$outFolder/dynamics_time.png
  if [ ! -f $dtOut ]
  then
    $buildFolder/../scripts/plant_dynamics.sh -q -f $resultFolder/global.dat -c "Plants;Time:y2" -o $dtOut
  else
    echo "Time dynamics '$dtOut' already generated"
  fi

  dsOut=$outFolder/dynamics_species.png
  if [ ! -f $dsOut ]
  then
    $buildFolder/../scripts/plant_dynamics.sh -q -f $resultFolder/global.dat -c "Plants;Species:y2" -o $dsOut
  else
    echo "Species dynamics '$dsOut' already generated"
  fi
fi

echo
