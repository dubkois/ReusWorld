#!/usr/bin/awk -f

BEGIN {
  if (ARGV[1] == "") {
    print "No input file provided. Aborting"
    exit 1
  }
    
  if (winsize == "")  winsize=10
  ofile=ARGV[1]".sw"winsize
  split(sum,a_sum,",")
  split(avg,a_avg,",")
  
  for (i in a_sum) {
    a_cols[j] = a_sum[i]
    a_types[j] = 0
    j++
  }
  for (i in a_avg) {
    a_cols[j] = a_avg[i]
    a_types[j] = 1
    j++
  }
  
  printf "Writing ("
  for (i in a_cols) {
    t = a_types[i] ? "avg" : "sum"
    printf " %s(%s)", t, a_cols[i]
  }
  printf " ) to %s\n", ofile
}

NR==1 {
  for (i=1; i<=NF; i++)
    ix[$i] = i
  printf "%s%s", $1, OFS > ofile
  for (i in a_cols)
    printf "%s%s", a_cols[i], OFS >> ofile
  print "" >> ofile
}
NR > 1 {
#   print NR-1, "%", winsize, "=", (NR-1) % winsize
  l = (NR-1) % winsize
  if (l == 1) time=$1
  if (l != 0) {
    for (i in a_cols)
      sums[a_cols[i]] += $ix[a_cols[i]]
      
  } else {
    printf "%s%s", time, OFS >> ofile
    for (i in a_cols) {
      if (a_types[i])
        sums[a_cols[i]] /= winsize
      printf "%s%s", sums[a_cols[i]], OFS >> ofile
      sums[a_cols[i]] = 0
    }
    print "" >> ofile
  }
}
