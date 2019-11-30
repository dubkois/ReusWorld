#!/bin/bash

d=results/timelines/pve/
H="0.00 0.25 0.50 0.75 1.00"

for e in $d*/evaluation.dat
do
  f=$(echo $e | sed 's|.*/pve/\(.\)[^_]*_\(.*\)_\(h.*\)_\(w.*\)/.*|\1\2 \3 \4|')
  printf "%s " $f
  tail -n 1 $e | sed 's/y1\(.*\)d.* \(.*\)/\1 \2/'
done | \
awk -vT=.2 -vD="$d" -vHH="$H" -vWH="$H" '
{
  h = substr($2, 2);
  w = substr($3, 2);
  y = $4;
  s = 100 * $5;
  
  data[$1][h][w][1] = y;
  data[$1][h][w][2] = s;
  
  agg[$1][1] += y;
  agg[$1][2] += s;
}

END {
  split(HH, his)
  for (i in his) hi[his[i]] = 1
  
  split(WH, wis)
  for (i in wis) wi[wis[i]] = 1

  N = length(hi) * length(wi);

  A = D"/aggregate.dat";
  
  PROCINFO["sorted_in"] = "@ind_str_asc"
  for (r in data) {
    yO = D"/"r".times.dat"
    sO = D"/"r".scores.dat"
    
    printf "%s %.1f %+07.2f\n", r, agg[r][1] / N, agg[r][2] / N > A
    
    printf "%04d", 0 > yO;
    printf "%04d", 0 > sO;
    for (w in wi) {
      printf " %s", w > yO;
      printf " %s", w > sO;
    }
    printf "\n" > yO
    printf "\n" > sO
    
    for (h in hi) {
      printf "%s", h > yO;
      printf "%s", h > sO;
      
      for (w in wi) {
        printf " %03d", data[r][h][w][1] > yO
        printf " %+07.2f", data[r][h][w][2] > sO
#         printf "%s %s %s: %d %.2f\n", r, h, w, data[r][h][w][1], data[r][h][w][2] > "/dev/stderr";
      }
      
      printf "\n" > yO
      printf "\n" > sO
    }
  }
}'

for data in $d/*.{times,scores}.dat
do
  o=$(dirname $data)/$(basename $data .dat).png
  
  gnuplot -e "
  set term pngcairo size 1050,1050;
  set output '$o';
  set style textbox opaque noborder;
  set xlabel 'Water variation';
  set ylabel 'Temperature variation';
  plot '$data' matrix rowheaders columnheaders with image notitle, \
       '' matrix rowheaders columnheaders using 1:2:(\$3 != \$3 ? '' : sprintf(\"% 5.1f\",\$3)) with labels boxed notitle;"
#   break
done
