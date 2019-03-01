#!/bin/bash

show_help(){
  echo "Usage: $0 -f <file> -c <columns> -p"
  echo "       -f <file> the datafile to use"
  echo "       -c <columns> columns specifiation (see below)"
  echo "       -p persist"
  echo "       -l <n> redraw graph every n seconds"
  echo "       -o <file> output file (implies no loop and no persist)"
  echo 
  echo "Columns specifiation:"
  echo "  N, a column name"
  echo "  exp(N1,...), an arithmetic expression (recognised by gnuplot) using any number of column names"
  echo "  C = N|exp(N1,...)[:y2], additionally specify to plot on the right y axis"
  echo "  columns = C[;C]*"  
}

# A POSIX variable
OPTIND=1         # Reset in case getopts has been used previously in the shell.

file=""
columns=""
persist=""
loop=""
outfile=""
while getopts "h?c:f:pl:o:" opt; do
    case "$opt" in
    h|\?)
        show_help
        exit 0
        ;;
    c)  columns=$OPTARG
        ;;
    f)  file=$OPTARG
        ;;
    p)  persist="-"
        ;;
    l)  loop="eval(loop($OPTARG))"
        ;;
    o)  outfile="set term pngcairo size 1680,1050;
set output '$OPTARG';
"
        ;;
    esac
done

if [ "$outfile" ]
then
  persist=""
  loop=""
fi

echo "   file: '$file'"
echo "columns: '$columns'"

lines=$(($(wc -l $file | cut -d ' ' -f 1) - 1))
tics=8

cmd="set datafile separator ' ';
set ytics nomirror;
set y2tics nomirror;
set key autotitle columnhead;"

if [ "$outfile" ]
then
  cmd="$cmd
$outfile"
else
  cmd="$cmd
loop(x) = 'while (1) { linesPerTic=ticsEvery(0); replot; pause '.x.'; };';"
fi

cmd="$cmd
xticsCount=$tics;
ticsEvery(x) = (system(\"wc -l $file | cut -d ' ' -f 1\") - 1) / xticsCount;
linesPerTic=ticsEvery(0);
plot '$file' using (0/0):xtic(int(\$0)%linesPerTic == 0 ? stringcolumn(1) : 0/0) notitle"

echo "reading columns specifications"
IFS=';' read -r -a columnsArray <<< $columns
for elt in "${columnsArray[@]}"
do
    W="A-Za-z_\{\}"
    y=$(cut -s -d: -f 2 <<< $elt)
    [ "$y" == "" ] && y="y1"
    gp_elt=$(cut -d: -f 1 <<< $elt | sed "s/\([^$W]*\)\([$W]\+\)\([^$W]*\)/\1column(\"\2\")\3/g")
    printf "$elt >> $gp_elt : $y\n"
    
    cmd="$cmd,
     '' using ($gp_elt) axes x1$y with lines title '$(cut -d: -f1 <<< $elt)'"
done

cmd="$cmd;"
if [ "$loop" ]
then
  cmd="$cmd
  $loop"
fi

printf "%s\n" "$cmd"

gnuplot -e "$cmd" --persist $persist

# plot for [i=1:words(lcols)] file using (column(word(lcols, i))) with lines title columnheader, \
#      for [i=1:words(rcols)] file using (column(word(rcols, i))) axes x1y2 with lines title columnheader
