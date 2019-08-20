#!/bin/sh

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
}

folder=""
samples=100
dim=2

# A POSIX variable
OPTIND=1         # Reset in case getopts has been used previously in the shell.

while getopts "h?f:s:d:" opt; do
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
  esac
done

if [ ! -d $folder ]
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
    for t in global topology temperature hygrometry; do awk -v s=$samples 'NR%s == 1' $e/$t.dat >> $folder/$t.dat; done
    printf "Aggregated data for $e\r"
  done
  echo
fi

./scripts/plant_dynamics.sh -q -f $folder/global.dat -c "Plants;Plants-Seeds;Seeds;Time:y2" -o $folder/pt_dynamics_summary.png
./scripts/plant_dynamics.sh -q -f $folder/global.dat -c "Organs/Plants;Time:y2" -o $folder/ot_dynamics_summary.png
./scripts/plant_dynamics.sh -q -f $folder/global.dat -c "ASpecies;CSpecies;Time:y2" -o $folder/st_dynamics_summary.png

if [ -z "$agg" ]
then
  ./scripts/controller_to_pdf.sh $folder/controller.tex
  dot -Tpng $folder/controller.dot -o $folder/controller_dot.png
fi

for t in topology temperature hygrometry
do
  echo "Plotting $folder/$t.dat"
  ./scripts/plot_voxels.sh -f $folder/$t.dat -d $dim
done

echo "Collating summaries"
if [ -z "$agg" ]
then
  montage -tile 3x3 $folder/controller_tex.png null: null: \
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
# xdg-open $folder/summary.png

