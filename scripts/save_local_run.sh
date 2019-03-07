#!/bin/bash

if [ $# -ne 1 ]
then
  echo "Usage: $0 <folder>"
  echo "       Moves data from last run into <folder>"
  exit 1
fi

if [ -d "$1" ]
then
  echo "ERROR: '$1' already exists. Aborting"
  exit 2
fi

mkdir -vp $1 $1/configs $1/saves

mv -v autosaves/* $1/autosaves/
cp -v configs/*.config $1/configs/
mv -v global.dat phylogeny.ptree.json $1/
mv -vf log $1
