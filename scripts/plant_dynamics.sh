#!/bin/bash

show_help(){
  echo "Usage: $0 -f <file> -c <columns> [-p] [-l] [-o <file>] [-q] [-g]"
  echo "       -f <file> the datafile to use"
  echo "       -c <columns> columns specifiation (see below)"
  echo "       -p persist"
  echo "       -l <n> redraw graph every n seconds"
  echo "       -o <file> output file (implies no loop and no persist)"
  echo "       -q Print a minimum of information"
  echo "       -g Additional gnuplot commands"
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
files=""
columns=""
persist=""
loop=""
outfile=""
verbose=""
gnuplotplus=""
while getopts "h?c:f:pl:o:qg:" opt; do
    case "$opt" in
    h|\?)
        show_help
        exit 0
        ;;
    c)  columns=$OPTARG
        ;;
    f)  file=$OPTARG
        [ ! -z "$files" ] && files="$files "
        files="$files$OPTARG"
        ;;
    p)  persist="-"
        ;;
    l)  loop="eval(loop($OPTARG))"
        ;;
    o)  outfile=$OPTARG
        
        case $outfile in
          *.png)
          output="  set term pngcairo size 1680,1050;"
          ;;
          *.pdf)
          output="  set term pdf size 11.2,7;"
          ;;
          *)
          echo "Unkown extension for output file '$outfile'. Aborting"
          exit 1
        esac
        
        output="$output
  set output '$outfile';
"
        ;;
    q)  verbose=no
        ;;
    g)  gnuplotplus=$OPTARG
        ;;
    esac
done

if [ "$outfile" ]
then
  persist=""
  loop=""
fi

if [ "$file" = "$files" ]
then
  [ -z "$verbose" ] && echo "   file: '$file'"
else
  [ -z "$verbose" ] && echo "  files: " $(echo $files | sed -e "s/ /' '/g" -e "s/^/'/" -e "s/$/'/")
  index=$(head -n 1 $file | awk -v header=$columns '{ for(i=1;i<=NF;i++) if ($i == header) print i}')
  echo "index: $index"
  
  i=0
  cfiles=""
  for file in $files
  do
    tmp=".$i.col"
    cut -d ' ' -f $index $file > $tmp && cfiles="$cfiles $tmp"
    i=$(expr $i + 1)
  done
  
  pasted=".n.col"
  paste -d " " $cfiles | awk -v cols=$columns 'BEGIN { print cols, cols "_lci", cols "_uci" } {
    sum=0;
    for (i=1; i<=NF; i++)  sum += $i;
    sum /= NF;
    std=0
    for (i=1; i<=NF; i++)  std += (sum-$i)*(sum-$i);
    std = sqrt(std / NF);
    Z=1.960;
    sr=sqrt(NF);
    print sum, sum - Z * std / sr, sum + Z * std / sr;
  }' > $pasted
  
  header=".header.col"
  cut -d ' ' -f 1 $file > $header

  decoyFile=".$columns.nh.col"
  paste -d " " $header $pasted > $decoyFile
  file=$decoyFile
#   columns="$columns; ${columns}_lci; ${columns}_uci"
fi

[ -z "$verbose" ] && echo "columns: '$columns'"

tics=6

cmd="  set datafile separator ' ';
  set ytics nomirror;
  set y2tics nomirror;
  set key autotitle columnhead;
  set style fill solid .25;" 

if [ "$gnuplotplus" ]
then
  cmd="$cmd
  
$gnuplotplus;  
"
fi

if [ "$outfile" ]
then
  cmd="$cmd
$output"
else
  cmd="$cmd
  loop(x) = 'while (1) { eval(setupXTics(0)); replot; pause '.x.'; };';"
fi

# rows=$(wc -l $file | cut -d ' ' -f 1)
# stride=$(($rows / 5))
# tics=$(cut -d ' ' -f 1 $file | awk -v s=$stride 'NR % s == 1 { printf "\"%s\" %d\n", $0, NR }' | paste -sd "," -)
# tics="$tics, \"y1000d00h0\" $(cat $file | wc -l)-1" # That's ugly but what the hell...

cmd="$cmd
  rows(x) = system('cat $file | wc -l');
  stride(x) = rows(x) / ($tics - 1);
  computeXTics(x) = system('cut -d \" \" -f 1 $file | awk -v s='.stride(0).' \"NR % s == 2 { print \\\$0, NR } END { print \\\"y1000d00h0\\\", NR-2 }\"');
  setupXTics(x) = 'tics=computeXTics('.x.'); set for [i=1:words(tics):2] xtics (word(tics, i) word(tics, i+1));';
  eval(setupXTics(0));
  plot '$file' using (0/0):(0/0) notitle"

# cmd="$cmd
#   xticsCount=$tics;
#   ticsEvery(x) = (system(\"wc -l $file | cut -d ' ' -f 1\") - 1) / xticsCount;
#   linesPerTic=ticsEvery(0);
#   plot '$file' using (0/0):xtic(int(\$0)%linesPerTic == 0 ? stringcolumn(1) : 0/0) notitle"
  
[ -z "$verbose" ] && printf "\nreading columns specifications\n"
IFS=';' read -r -a columnsArray <<< $columns
for elt in "${columnsArray[@]}"
do
    W="A-Za-z_\{\}"
    y=$(cut -s -d: -f 2 <<< $elt)
    [ "$y" == "" ] && y="y1"
    colname=$(cut -d: -f 1 <<< $elt)
    gp_elt=$(sed "s/\([^$W]*\)\([$W]\+\)\([^$W]*\)/\1column(\"\2\")\3/g" <<< $colname)
    [ -z "$verbose" ] && printf "$elt >> $gp_elt : $y\n"
    
    if [ "$file" != "$files" ]
    then
      cmd="$cmd,
       '' using 0:(column(\"${colname}_lci\")):(column(\"${colname}_uci\")) with filledcurves title '$colname confidence interval (95%)'"
    fi
    
    cmd="$cmd,
       '' using ($gp_elt) axes x1$y with lines title '$(cut -d: -f1 <<< $elt)'"
done

cmd="$cmd;"
if [ "$loop" ]
then
  cmd="$cmd
  $loop"
fi

if [ -z "$verbose" ]
then
  printf "\nGnuplot script:\n%s\n" "$cmd"
else
  printf "Plotting from '$file' using '$columns'"
  [ ! -z "$outfile" ] && printf " into '$outfile'"
  printf "\n"
fi

gnuplot -e "$cmd" --persist $persist
