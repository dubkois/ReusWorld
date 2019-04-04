#!/bin/bash

function finish {
  kill $(jobs -p)
}
trap finish EXIT

if [ $# -ne 1 ]
then
  echo "Usage: $0 <result-folder>"
  echo "       displays contents of sumup results in <result-folder>"
  exit 10
fi

resultFolder=$(readlink -f $1/sumup)
if [ ! -d "$resultFolder" ]
then
  echo "ERROR: result folder '$1' does not exist. Aborting"
  exit 1
fi

cd $resultFolder/
feh --scale-down --auto-zoom --title "Dynamics: %n (%u/%l)" dynamics*.png &
feh --scale-down --auto-zoom --title "All morphologies: %n (%u/%l)" morphologies/*.png &
feh --scale-down --auto-zoom --title "Common morphologies: %n (%u/%l)" $(ls morphologies/*.png | sed '/.*_00[0-9].[0-9][0-9][0-9]p.*/d') &
gwenview phylogeny.svg &

wait
