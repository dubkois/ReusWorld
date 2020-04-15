#!/bin/sh

usage(){
  echo "Usage: $0 -f <voxels.dat> [-d=2|-d=3] [-l v] [-u v] [-e ext] [-H] [-p] [-g gnuplot-commands]"
  echo "       -d Plot as 3d or projected map (2d)"
  echo "       -p Keep the graph interactive"
  echo "       -e Output file type (if not interactive)"
  echo "       -H Use horizontal time instead of default vertical"
  echo "       -l,-u set the lower and upper bounds of the graph, respectively"
}

file=""
dim=2
ext="png"
persist=""
lower="*"
upper="*"

margs="1:2:3"
Y="y"
X="x"

# A POSIX variable
OPTIND=1         # Reset in case getopts has been used previously in the shell.

while getopts "h?f:d:l:u:e:g:pH" opt; do
  case "$opt" in
  h|\?)
      usage
      exit 0
      ;;
  f)  file=$OPTARG
      ;;
  d)  dim=$OPTARG
      ;;
  e)  ext=$OPTARG
      ;;
  l)  lower=$OPTARG
      ;;
  u)  upper=$OPTARG
      ;;
  g)  gcommands=$OPTARG
      ;;
  H)  margs="2:1:3"
      Y="x"
      X="y"
      ;;
  p)  persist="-"
  esac
done

if [ ! -f "$file" ]
then
  echo "'$file' does not exist"
  usage
  exit 1
fi

if [ "$dim" -lt 2 -o "$dim" -gt 3 ]
then
  echo "Wrong dimension. '$dim' is neither 2 nor 3"
  usage
  exit 2
fi

if [ "$dim" -eq 2 ]
then  
  map="set pm3d map;"
  plot="plot"
  pstyle="image"
else
  map=""
  plot="splot"
  pstyle="pm3d"
fi

if [ -z "$persist" ]
then
  case "$ext" in
    pdf)  term="pdfcairo size 11.2,7;"
          ;;
      
    *)  term="pngcairo size 1680,1050;"
        ext="png"
        ;;
  esac
  output="
  set term $term;
  set output '$(dirname $file)/$(basename $file .dat)_${dim}D.$ext';"
fi

rows=$(wc -l $file | cut -d ' ' -f 1)
stride=$(($rows / 5))
tics="'200' 2000, '400' 4000, '600' 6000, '800' 8000, '1000' 10000-1"
# tics=$(cut -d ' ' -f 1 $file | awk -v s=$stride 'NR % s == 1 { printf "\"%s\" %d\n", $0, NR }' | paste -sd "," -)
# tics="$tics, \"y1000d00h0\" $(cat $file | wc -l)-1" # That's ugly but what the hell...
  
gnuplot -e "
  set ${X}label 'X';
  set ${Y}label 'Time';
  set ${Y}tics ($tics);
  set tics nomirror out;
  set zrange [$lower:$upper];
  set cbrange [$lower:$upper];
  set autoscale fix;
  $output
  $map
  $gcommands;
  $plot '< cut -d \" \" -f2- $file' using $margs matrix with $pstyle notitle " -p $persist
  
#   set title '$(basename $file .dat)';
