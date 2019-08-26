#!/bin/bash

usage(){
  echo "Usage: $0 -f result-folder [OPTIONS]"
  echo
  echo " Generates a summary image of the simulation under result-folder"
  echo " If result-folder corresponds to the simulation root (i.e. the folder undr which every alternative is stored), runs in aggregate mode"
  echo " Otherwise generates data for the given folder"
  echo 
  echo "OPTIONS"
  echo "       -s S, only use 1 in every S lines (in aggregate mode, defaults to 100)"
  echo "       -d D, Plot voxels in D dimensions (2 or 3, defaults to 2)"
  echo "       -p, purge result-folder from summary images (useful for clean-up or to reevaluate a folder)"
}

folder=""
samples=100
dim=2

# A POSIX variable
OPTIND=1         # Reset in case getopts has been used previously in the shell.

while getopts "h?f:s:d:p" opt; do
  case "$opt" in
  h|\?)
      show_help
      exit 0
      ;;
  f)  folder=$OPTARG
      ;;
  s)  samples=$OPTARG
      ;;
  d)  dim=$OPTARG
      ;;
  p)  rm -v $folder/*summary.png $folder/*[23]D.png
      ;;
  esac
done

if [ ! -d "$folder" ]
then
  echo "'$folder' is not a folder"
  usage
  exit 1
fi

if [ $samples -lt 2 ]
then
  echo "Invalid sample size '$samples' < 2"
  usage
  exit 2
fi

if [ $dim -lt 2 -o $dim -gt 3 ]
then
  echo "Invalid dimension '$dim' neither 2 nor 3"
  usage
  exit 3
fi

if [ -f $folder/summary.png ]
then
  echo "Skipping previously processed folder '$folder'"
  exit 0
fi

agg=""
firstFolder=$(find $folder/ -path "*results/e*_r" | grep "e[0]*_r")
if [ -d "$firstFolder" ]
then
  # run folder > Agreggate first
  agg="yes"
  echo "Running in aggregate mode"

  lastFolder=$(ls -d $folder/results/e*_r | tail -1)
  
  for t in global topology temperature hygrometry; do rm -fv $folder/$t.dat; done
  head -1 $firstFolder/global.dat > $folder/global.dat
  for e in $folder/results/e*_r
  do
    for t in global topology temperature hygrometry; do awk -v s=$samples 'NR%s == 2' $e/$t.dat >> $folder/$t.dat; done
    printf "Aggregated data for $e\r"
  done
  echo
fi

pfdsFile=$folder/pt_dynamics_summary.png
[ ! -f $pfdsFile ] && ./scripts/plant_dynamics.sh -q -f $folder/global.dat -c "Plants;Plants-Seeds;Seeds;Time:y2" -o $pfdsFile

otdsFile=$folder/ot_dynamics_summary.png
[ ! -f $otdsFile ] && ./scripts/plant_dynamics.sh -q -f $folder/global.dat -c "Organs/Plants;Time:y2" -o $otdsFile

stdsFile=$folder/st_dynamics_summary.png
[ ! -f $stdsFile ] && ./scripts/plant_dynamics.sh -q -f $folder/global.dat -c "ASpecies;CSpecies;Time:y2" -o $stdsFile

if [ -z "$agg" -a ! -f $folder/controller_tex.png ]
then
  ./scripts/controller_to_pdf.sh $folder/controller.tex
#   dot -Tpng $folder/controller.dot -o $folder/controller_dot.png
fi

declare -A lranges=( [topology]=-25 [temperature]=-20 [hygrometry]=0 )
declare -A uranges=( [topology]=25 [temperature]=40 [hygrometry]=1 )
for t in topology temperature hygrometry
do
  if [ ! -f $folder/${t}_${dim}D.png ]
  then
    echo "Plotting $folder/$t.dat"
    ./scripts/plot_voxels.sh -f $folder/$t.dat -d $dim -l ${lranges[$t]} -u ${uranges[$t]}
  fi
done

echo "Collating summaries"
if [ -z "$agg" ]
then
  montage -tile 3x3 'null:[1680x10!]' $folder/controller_tex.png null: \
                    $folder/pt_dynamics_summary.png $folder/ot_dynamics_summary.png $folder/st_dynamics_summary.png \
                    $folder/topology_${dim}D.png $folder/temperature_${dim}D.png $folder/hygrometry_${dim}D.png \
          -geometry '+2+2' $folder/summary.png
  echo "Generated summary for $folder"
  
else
  montage -tile 3x2 $folder/pt_dynamics_summary.png $folder/ot_dynamics_summary.png $folder/st_dynamics_summary.png \
                    $folder/topology_${dim}D.png $folder/temperature_${dim}D.png $folder/hygrometry_${dim}D.png \
          -geometry '1680x1050+2+2<' $folder/summary.png
  echo "Generated summary for $folder from $firstFolder to $lastFolder"
fi
