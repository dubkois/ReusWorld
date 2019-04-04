#!/bin/bash

usage(){
  echo "Usage: $0 <folder>"
}

if [ $# -ne 1 ]
then
  echo "Invalid number of arguments"
  usage
  exit 10
fi

if [ ! -d "$1" ]
then
  echo "'$1' is not a valid folder"
  usage
  exit 1
fi

folder=$1

lastSave=$(find $folder/autosaves/ -name "*.save.ubjson" -printf "%T@ %p\n" | sort -n | cut -d " " -f 2- | tail -n 1)
lastSaveYear=$(echo $lastSave | sed 's/.*y\([0-9][0-9]*\).*/\1/')
popCounts=${lastSave/save.ubjson/density.dat}

if [ ! -f $popCounts ]
then
  # Extract population count per species
  ./build_release/analyzer -l $lastSave --density-histogram 2>/dev/null
fi

time=$(head -n 3 $popCounts | tail -2 | cut -d ' ' -f4 | sort -gr | head -n 1)
year=$((($time + 1000 - 1) / 1000))

compatDat=$folder/compatibilities.dat
if [ ! -f $compatDat ]
then
  # Get most populated two species and latest appearance
  sids=$(head -n 3 $popCounts | tail -2 | cut -d ' ' -f1)
  sids=$(echo $sids | tr " " ",")

  # Generate compatibility data
  echo "Generating compatibility data for $sids from year $year"
  for i in $(seq $year $lastSaveYear)
  do
    ./build_release/analyzer -l $folder/autosaves/y$i.save.ubjson --compatibility-matrix="$sids" 2>/dev/null | grep "plotData" | cut -d ' ' -f 2-
  done | tee $compatDat

  grep -v "^nan" $compatDat | sort | uniq > $folder/.compatibilities.dat
  mv $folder/.compatibilities.dat $compatDat
fi

compatPlot=$folder/compatibilities.png
if [ ! -f $compatPlot ]
then

  gnuplot -e "
set term pngcairo;
set output '$compatPlot';
set key autotitle columnhead;
xticsCount=8;
ticsEvery(x) = (system(\"wc -l $compatDat | cut -d ' ' -f 1\") - 1) / xticsCount;
linesPerTic=ticsEvery(0);
plot '$compatDat' using (0/0):xtic(int(\$0)%linesPerTic == 0 ? stringcolumn(1) : 0/0) notitle,
     for [i=2:5] '' using i with lines ti col;"
  echo "Generated $compatPlot"
fi

ttestResult=$folder/ttestResult
if [ ! -f $ttestResult ]
then
  echo "[$(date)] Paired TTest results for '$folder'" | tee $ttestResult
  echo "Population counts:" | tee -a $ttestResult
  head -n 3 $popCounts | tee -a $ttestResult
  echo "Cohabitation: $(($lastSaveYear - $year)) years" | tee -a $ttestResult
  for i in 3 4
  do
    for j in 2 5
    do
      printf "TTest2($(head -n 1 $compatDat | cut -d ' ' -f $i), $(head -n 1 $compatDat | cut -d ' ' -f $j)) "
      ./scripts/ttest2.octave $compatDat $i $j
#       printf "\n"
    done
  done | tee -a $ttestResult
fi
