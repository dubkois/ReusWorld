#!/usr/bin/gnuplot --persist

print "      script : ", ARG0
print " input file  : ", ARG1
print "   bandwidth : ", ARG2
print " output file : ", ARG3
print "     commands: ", ARG4

bwidth=(ARGC>=2)? ARG2 : 1
# 
# nsamp = 9000
# jitter(i,mu1,mu2,mu3, dev1,dev2,dev3) = \
#     (i%4 == 0) ? mu1 +  dev1*invnorm(rand(0)) \
#   : (i%4 == 1) ? mu2 +  dev2*invnorm(rand(0)) \
#   :              mu3 +  dev3*invnorm(rand(0))
# set print "test.dat"
# do for [i=1:nsamp] {
#     c = int(rand(0)*3)
#     if (c == 0) { y = jitter(i, 120, 300, 400, 10, 40, 70) }
#     if (c == 1) { y = jitter(i, 300, 400, 120, 70, 10, 40) }
#     if (c == 2) { y = jitter(i, 400, 120, 300, 40, 70, 10) }
#     print sprintf("%g %d ", y, c)
# }
# unset print
# ARG1="test.dat"

sep=' '
S=system("head -n 1 ".ARG1." | wc -w | cut -d ' ' -f 1")-1
names=system("cut -d '".sep."' -f1 ".ARG1." | sort | uniq")
name(i)=word(names,i)
N=words(names)

pointsfile(s,j)=".".s.".".j.".vp_extracted_points"
pointsfile_i(i,j)=pointsfile(name(i),j)
extractpoints(s,j)=system("grep '".s."' ".ARG1." | cut -d '".sep."' -f".(j+1)." > ".pointsfile(s,j))

print S, " sets and ", N, " columns"

tmp(n,s)=".".n.".".s.".kdensity"
tmp_i(i,s)=tmp(word(names,i),s)

do for [i=1:N] {
  do for [j=1:S] {
    s=word(names,i)
    print "set ".i.".".j.": ".s
    res=extractpoints(s,j)
    pointscount=system("wc -l ".pointsfile(s,j)." | cut -d ' ' -f1")
    
    set table tmp(s, j)
    plot pointsfile(s,j) using 1:((1.*bwidth)/pointscount) \
      smooth kdensity bandwidth bwidth with filledcurves above y lt 9 title s
    unset table
    unset key

#   print $kdensity
  }
}

if (ARGC >= 3 && ARG3 ne "") {
  set terminal pdf size 6,3
  set output ARG3
}

w=1
if (ARGC >= 4) {
  eval ARG4
}

set grid ytics mytics lw 1, lc rgb "gray"

set xtics ()
set for [i=1:N] xtics add (sprintf("%s", word(names, i)) i)
set xtic scale 0

set mytics 5

set boxwidth 0.025
set style boxplot nooutliers fraction 0.95
# set errorbars lt black lw 1
# unset bars

if (S>1) {
  set style fill pattern border -1
  set key
} else {
  set style fill solid border -1
  cols=""
}

gfs(j) = S>1 ? j : 7

plot for [i=1:N] for [j=1:S] tmp_i(i,j) using (i + w*bwidth*$2):1 with filledcurve x=i ls i fs pattern j notitle,\
     for [i=1:N] for [j=1:S] tmp_i(i,j) using (i + w*bwidth*$2):1 with lines ls i notitle,\
     for [i=1:N] for [j=1:S] tmp_i(i,j) using (i - w*bwidth*$2):1 with filledcurve x=i ls i fs pattern j notitle,\
     for [i=1:N] for [j=1:S] tmp_i(i,j) using (i - w*bwidth*$2):1 with lines ls i notitle,\
     for [i=1:N] for [j=1:S] pointsfile_i(i,j) using (i):1 with boxplot fc "white" notitle, \
     for [j=1:S] NaN with boxes ls 1 fillstyle pattern j title word(cols, j)

# print GPVAL_DATA_Y_MIN, ", ", GPVAL_DATA_Y_MAX

system("rm .*.kdensity .*.vp_extracted_points")
