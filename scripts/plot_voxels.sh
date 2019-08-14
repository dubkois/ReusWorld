#!/bin/sh

usage(){
  echo "Usage: $0 <voxels.dat> [2|3]"
  echo "       second argument is for whether to plot as 3d of projected map (2d)"
}

if [ $# -lt 1 -o $# -gt 2 ]
then
  usage
  exit 10
fi

if [ ! -f $1 ]
then
  usage
  exit 1
fi

if [ "$2" -lt 2 -o "$2" -gt 3 ]
then
  echo "Wrong dimension. '$2' is neither 2 nor 3"
  usage
  exit 2
fi

map=""
[ "$2" -eq 2 ] && map="set pm3d map"

rows=$(wc -l $1 | cut -d ' ' -f 1)
stride=$((($rows) / 5))
ytics=$(cut -d ' ' -f 1 $1 | awk -v s=$stride 'NR % s == 1 { printf "\"%s\" %d\n", $0, NR }' | paste -sd "," -)

gnuplot -e "
  set xlabel 'X';
  set ylabel 'Time';
  set ytics ($ytics);
  set title '$(basename $1 .dat)';
  set term pngcairo size 1680,1050;
  set output '$(dirname $1)/$(basename $1 .dat).png';
  splot '< cut -d \" \" -f2- $1' matrix with pm3d notitle " -p
  
