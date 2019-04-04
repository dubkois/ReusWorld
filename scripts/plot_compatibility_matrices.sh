#!/bin/bash

if [ $# -ne 1 ]
then
  echo "Usage: $0 <save-file>"
  echo "       Generates the compatibility matrices (absolute/relative) for the provided save file"
  exit 10
fi

if [ ! -f "$1" ]
then
  echo "ERROR: '$1' does not exist. Aborting"
  exit 1
fi

basefile=${1%.save.ubjson}
datafile=$basefile.compatibilities
cmfile=$datafile.absolute
rmfile=$datafile.relative

if [ ! -f "$datafile" ]
then
  ./build_release/analyzer -l $1 --compatibility-matrix="" | tee >(tr "\r" "\n" | grep "^[^[]" > $datafile) 
fi

grep "^cm" $datafile | tr -s " " | tr "-" "0" | cut -d ' ' -f2- > $cmfile
grep "^rc" $datafile | tr -s " " | tr "-" "0" | cut -d ' ' -f2- > $rmfile

gnuplot -e "
set term pngcairo size 1680,1050;
set output '$cmfile.png';
set autoscale xfix;
set autoscale yfix;
set autoscale cbfix;
plot '$cmfile' matrix nonuniform with image notitle;"

gnuplot -e "
set term pngcairo size 1680,1050;
set output '$rmfile.png';
set autoscale xfix;
set autoscale yfix;
set autoscale cbfix;
set logscale cb;
plot '$rmfile' matrix nonuniform with image notitle;"

rm $cmfile $rmfile
