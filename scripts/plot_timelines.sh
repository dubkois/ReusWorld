#!/bin/bash

usage(){
  echo "Usage: $0 <timelines data>"
}

if [ $# -ne 1 ]
then
  usage
  exit 10
fi

timelinesFile=$1
if [ ! -f "$timelinesFile" ]
then
  usage
  exit 1
fi

fA=1
fB=2

cA=$(($fA + 3))
cB=$(($fB + 3))

epochs=$(cut -d ' ' -f 1 $timelinesFile | tail -n +2 | uniq)
last=$(echo $epochs | tr " " "\n" | tail -1)

read hA hB <<< $(head -1 $timelinesFile | cut -d ' ' -f $cA,$cB)

folder=$(dirname $timelinesFile)

for i in $epochs
do
  printf "Processing epoch $i...\r"
  vdata=$folder/.vectordata.$i.dat
  [ ! -f $vdata ] && \
  awk -v i=$i -v cA=$cA -v cB=$cB '$1 == (i-1) && $3 == 1 { org[0]=$cA; org[1]=$cB; org[2]=$1; } $1 == i { print org[0], org[1], org[2], $cA - org[0], $cB - org[1], $1 - org[2], $3 }' $timelinesFile > $vdata
done

gnuplot -p -e "
  F='$timelinesFile';
  rgb(r,g,b) = int(r)*65536 + int(g)*256 + int(b);
  color(a) = ((a == 1)? rgb(0,0,0) : rgb(196,196,196));
  set xyplane 0;
  set xlabel '$hA';
  set ylabel '$hB';
  set zlabel 'Epochs' rotate parallel;
  splot for [i=1:$last] '$folder/.vectordata.'.i.'.dat' using 1:2:3:4:5:6:(color(\$7)) with vectors \
                                                    lc rgbcolor variable nohead notitle" -
                                                    
rm $folder/.vectordata.*.dat
