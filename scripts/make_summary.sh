#!/bin/sh

usage(){
  echo "Usage: $0 <result-folder> <sample-every>"
}

echo $@
if [ $# -lt 1 -o $# -gt 2 ]
then
  usage
  exit 10
fi

if [ ! -d $1 ]
then
  usage
  exit 1
fi

samples=1
[ $# -eq 2 ] && samples=$2

agg=""
firstFolder=$(ls -d $1/results/e*_r | grep "e[0]*_r")
if [ -d $firstFolder ]
then
  # run folder > Agreggate first
  agg="yes"
  echo "Running in aggregate mode"

  for t in global topology temperature hygrometry; do rm -fv $1/$t.dat; done
  head -1 $firstFolder/global.dat > $1/global.dat
  for e in $1/results/e*_r
  do
    for t in global topology temperature hygrometry; do awk -v s=$samples 'NR%s == 1' $e/$t.dat >> $1/$t.dat; done
    printf "Aggregated data for $e\r"
  done
  echo
fi

./scripts/plant_dynamics.sh -q -f $1/global.dat -c "Plants;Plants-Seeds;Seeds;Time:y2" -o $1/pt_dynamics_summary.png
./scripts/plant_dynamics.sh -q -f $1/global.dat -c "Organs/Plants;Time:y2" -o $1/ot_dynamics_summary.png
./scripts/plant_dynamics.sh -q -f $1/global.dat -c "ASpecies;CSpecies;Time:y2" -o $1/st_dynamics_summary.png

if [ -z "$agg" ]
then
  ./scripts/controller_to_pdf.sh $1/controller.tex
  dot -Tpng $1/controller.dot -o $1/controller_dot.png
fi

for t in topology temperature hygrometry
do
  echo "Plotting $1/$t.dat"
  ./scripts/plot_voxels.sh -f $1/$t.dat -d 3
done

echo "Collating summaries"
if [ -z "$agg" ]
then
  montage -tile 3x3 $1/controller_tex.png null: null: \
                    $1/pt_dynamics_summary.png $1/ot_dynamics_summary.png $1/st_dynamics_summary.png \
                    $1/topology.png $1/temperature.png $1/hygrometry.png \
          -geometry '+2+2' $1/summary.png
else
  montage -tile 3x2 $1/pt_dynamics_summary.png $1/ot_dynamics_summary.png $1/st_dynamics_summary.png \
                    $1/topology.png $1/temperature.png $1/hygrometry.png \
          -geometry '1680x1050+2+2<' $1/summary.png
fi
# xdg-open $1/summary.png

