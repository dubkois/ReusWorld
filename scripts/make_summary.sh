#!/bin/sh

usage(){
  echo "Usage: $0 <result-folder>"
}

echo $@
if [ $# -ne 1 ]
then
  usage
  exit 10
fi

if [ ! -d $1 ]
then
  usage
  exit 1
fi

agg=""
if [ -d $1/e00_r ]
then
  # run folder > Agreggate first
  agg="yes"
  for t in global topology temperature hygrometry; do rm -fv $1/$t.dat; done
  head -1 $1/e00_r/global.dat > $1/global.dat
  for e in $1/e*_r
  do
    for t in global topology temperature hygrometry; do cat $e/$t.dat >> $1/$t.dat; done
  done
fi

./scripts/plant_dynamics.sh -q -f $1/global.dat -c "Plants;Species:y2" -o $1/dynamics_summary.png

if [ -z "$agg" ]
then
  ./scripts/controller_to_pdf.sh $1/controller.tex
  dot -Tpng $1/controller.dot -o $1/controller_dot.png
fi

for t in topology temperature hygrometry
do
  ./scripts/plot_voxels.sh $1/$t.dat
done

if [ -z "$agg" ]
then
  montage -tile 3x2 $1/controller_tex.png $1/dynamics_summary.png $1/controller_dot.png \
                    $1/topology.png $1/temperature.png $1/hygrometry.png\
          -geometry +2+2 $1/summary.png
else
  montage -tile 3x2 $1/dynamics_summary.png $1/topology.png $1/temperature.png $1/hygrometry.png -geometry +2+2 $1/summary.png
fi
# xdg-open $1/summary.png

