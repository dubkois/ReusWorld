#!/bin/sh

usage(){
  echo "Usage: $0 <voxels.dat>"
}

if [ $# -ne 1 ]
then
  usage
  exit 10
fi

if [ ! -f $1 ]
then
  usage
  exit 1
fi

rows=$(wc -l $1 | cut -d ' ' -f 1)
stride=$((($rows) / 5))
ytics=$(cut -d ' ' -f 1 $1 | awk -v s=$stride 'NR % s == 1 { printf "\"%s\" %d\n", $0, NR }' | paste -sd "," -)

gnuplot -e "
  set xlabel 'X';
  set ylabel 'Time';
  set ytics ($ytics);
  set term pngcairo size 1680,1050;
  set output '$(dirname $1)/$(basename $1 .dat).png';
  splot '< cut -d \" \" -f2- $1' matrix with pm3d title '$(basename $1 .dat)'" -p
  
