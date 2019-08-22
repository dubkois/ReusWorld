#!/bin/sh

usage(){
  echo "Usage: $0 -f <voxels.dat> [-d=2|-d=3] [-l v] [-u v] [-p]"
  echo "       -d for whether to plot as 3d of projected map (2d)"
  echo "       -p for whether to keep the graph interactive"
  echo "       -l,-u set the lower and upper bounds of the graph, respectively"
}

file=""
dim=2
persist=""
lower="*"
upper="*"

# A POSIX variable
OPTIND=1         # Reset in case getopts has been used previously in the shell.

while getopts "h?f:d:l:u:p" opt; do
  case "$opt" in
  h|\?)
      show_help
      exit 0
      ;;
  f)  file=$OPTARG
      ;;
  d)  dim=$OPTARG
      ;;
  l)  lower=$OPTARG
      ;;
  u)  upper=$OPTARG
      ;;
  p)  persist="-"
  esac
done

if [ ! -f "$file" ]
then
  usage
  exit 1
fi

if [ "$dim" -lt 2 -o "$dim" -gt 3 ]
then
  echo "Wrong dimension. '$dim' is neither 2 nor 3"
  usage
  exit 2
fi

map=""
[ "$dim" -eq 2 ] && map="set pm3d map;"

if [ -z "$persist" ]
then
  output="
  set term pngcairo size 1680,1050;
  set output '$(dirname $file)/$(basename $file .dat)_${dim}D.png';"
fi

rows=$(wc -l $file | cut -d ' ' -f 1)
stride=$(($rows / 5))
ytics=$(cut -d ' ' -f 1 $file | awk -v s=$stride 'NR % s == 1 { printf "\"%s\" %d\n", $0, NR }' | paste -sd "," -)

gnuplot -e "
  set xlabel 'X';
  set ylabel 'Time';
  set ytics ($ytics);
  set title '$(basename $file .dat)';
  set zrange [$lower:$upper];
  set cbrange [$lower:$upper];
  $output
  $map
  splot '< cut -d \" \" -f2- $file' matrix with pm3d notitle " -p $persist
  
