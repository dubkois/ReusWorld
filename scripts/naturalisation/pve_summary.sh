#!/bin/bash

d=results/timelines/pve/

for e in $d*/evaluation.dat
do
  f=$(echo $e | sed 's|.*/pve/\(.\)[^_]*_\(.*\)_\(h.*\)_\(w.*\)/.*|\1\2 \3 \4|')
  printf "%s " $f
  tail -n 1 $e | sed 's/y1\(.*\)d.* \(.*\)/\1 \2/'
done | \
awk -vN=25 -vT=.2 -vO="$d/paretos.data" '
function isDominated (lhs, rhs,      i) {
  better = false; 
  for (i=1; i<=2; i++) {
    if (lhs[i] < rhs[i])  return false;
    better = better || (rhs[i] < lhs[i]);
  }
  return better;
}

{
  counts[$1]++;
  i=counts[$1];
  data[$1][i][1] = substr($2, 2);
  data[$1][i][2] = substr($3, 2);
  
  for (j=4; j<=NF; j++)
    data[$1][i][j-1] = $j;
}

END {
#   for (r in counts) {
#     printf "%s:\n", r;
#     for (a in data[r]) {
#       printf "\t%s:\n", a;
#       for (i in data[r][a]) {
#         printf "\t\t%d: %s\n", i, data[r][a][i];
#       }
#     }
#   }
#     

  PROCINFO["sorted_in"] = "@ind_str_asc"
  for (r in counts) {
    if (counts[r] < N) continue;
    printf "Computing pareto front for %s\n", r;
    printf "\tCandidates:\n";

    delete front;
    frontSize=0;
    
    delete fdata;

    n=1;
    for (i=1; i<=N; i++) {
      if (data[r][i][4] < T)  continue;
      for (j in data[r][i]) fdata[r][n][j] = data[r][i][j];
      n++;
    }
    
    delete data[r];
    
    for (i=1; i<n; i++) {
      printf "\t\t%2d:", i;
      for (f in fdata[r][i]) printf " %s", fdata[r][i][f];
      printf "\n";
    
      dominated = false;
      
      for (j in front) {
#         printf "dominated = isDominated(fdata[%s][front[%s]], fdata[%s][%s])\n", r, j, r, i;
        dominated = isDominated(fdata[r][front[j]], fdata[r][i])
        if (dominated)  break;
      }

      if (!dominated) {
        for (j=i+1; j<n; j++) {
#           printf "dominated = isDominated(fdata[%s][%s], fdata[%s][%s])\n", r, j, r, i;
          dominated = isDominated(fdata[r][j], fdata[r][i])
          if (dominated)  break;
        }
      }
      
      if (!dominated)
        front[frontSize++] = i;
    }
    
    printf "\t%d items in front\n", length(front);
    for (p in front) {
      printf "\t\t%2d", p;
      printf "%s %02d", r, p > O;
      printf ":";
      for (i in fdata[r][front[p]]) {
        printf " %s", fdata[r][front[p]][i];
        printf " %s", fdata[r][front[p]][i] > O;
      }
      printf "\n";
      printf "\n" > O;
    }
    printf "\n";
  }
}'

cat $d/paretos.data
