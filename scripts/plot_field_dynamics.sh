#!/bin/bash

show_help(){
  echo "Usage: $0 -f field-name -i inputfile -s step-size [-o outfile] [-t title] [-p] [-v]"
  echo
  echo "       -f field-name, name of the field being rendered"
  echo "       -i inputfile, file containing the formatted data as produced by $(dirname $0)/field_dynamics.sh"
  echo "       -s step-size, duration of a single box in simulated time"
  echo "       -o outfile, filepath for the produced graphic (implies not persist)"
  echo "       -t title, graph title"
  echo "       -p, Whether to maintain the graph active in gnuplot (implies no output file)"
  echo "       -v, Whether to be verbose"
}

OPTIND=1         # Reset in case getopts has been used previously in the shell.
field="?"
infile=""
outfile=""
title=""
persist=""
boxwidth=1
verbose="no"

while getopts "h?f:i:s:o:t:pv" opt; do
  case "$opt" in
  h|\?)
      show_help
      exit 0
      ;;
  f)  field=$OPTARG
      ;;
  i)  infile=$OPTARG
      ;;
  s)  boxwidth=$OPTARG
      ;;
  o)  outfile=$OPTARG
      persist=""
      ;;
  t)  title=$OPTARG
      ;;
  p)  persist="-"
      ;;
  v)  verbose=""
      ;;
  esac
done

if [ -z "$field" ]
then
  echo "Unspecified field name"
  exit 1
fi

if [ ! -f "$infile" ]
then
  echo "'$infile' is not a regular file"
  exit 2
fi

printf "\n%80s\n" " " | tr ' ' '-'
printf "Generating graph for field $field\n"

globalmin=$(cut -d ' ' -f 2 $infile | LC_ALL=C sort -g | head -1)
[ -z "$verbose" ] && echo "global min: $globalmin"

globalmax=$(cut -d ' ' -f 2 $infile | LC_ALL=C sort -gr | head -1)
[ -z "$verbose" ] && echo "global max: $globalmax"

offset=0
ytics=""
if awk "BEGIN {exit !($globalmin < 0)}"
then
  offset=$(awk "BEGIN { print -1*$globalmin }" )
  [ -z "$verbose" ] && echo "Offsetting by $offset"
  
  # Compute un-offsetted ytics
  # Inspired by gnuplot/axis.c:678 quantize_normal_tics (yes it is kind of ugly. But! It works!!!)
  ytics=$(awk -vmin=$globalmin -vmax=$globalmax -vtics=8 '
    function floor (x) { return int(x); }
    function ceil (x) { return int(x+1); }
    BEGIN {
      r=max-min;
      power=10**floor(log(r)/log(10));
      xnorm = r / power;
      posns = 20 / xnorm;
      
      if (posns > 40)       tics=.05;
      else if (posns > 20)  tics=.1;
      else if (posns > 10)  tics=.2;
      else if (posns > 4)   tics=.5;
      else if (posns > 2)   tics=1;
      else if (posns > .5)  tics=2;
      else                  tics=ceil(xnorm);
      
      ticstep = tics * power;
      
      start=ticstep * floor(min / ticstep);
      end=ticstep * ceil(max / ticstep);
      
      for (s=start; s<=end; s+=ticstep) {
        if (s == 0) s = 0;  # To make sure zero is zero (remove negative zero)
        printf "\"%g\" %g\n", s, s-min
      }
    }' | paste -sd,)
  [ -z "$verbose" ] && echo "     ytics: $ytics"
fi

[ -z "$verbose" ] && echo "  boxwidth: $boxwidth"

cmd="set style fill solid 1 noborder;
set autoscale fix;" 

if [ ! -z "$outfile" ]
then
  ext=$(awk -F. '{print $NF}' <<< "$outfile")
  case "$ext" in
    pdf)  output="set term pdfcairo size 11.2,7"
          ;;
    *)    output="set term pngcairo size 1680,1050"
          ;;
  esac
    
  output="$output; set output '$outfile';"
fi

cmd="$cmd
set format x \"y%.0fd00h0\";
set xtics add ('y0d00h0' 4);
set format y \"%5.2g\";
set ytics ($ytics);
set xlabel 'Time';
set style fill solid 1.0 noborder;
$output"

if [ "$title" ]
then
  cmd="$cmd
set title '$title';"
fi

cmd="$cmd
icolor(r) = int((1-r)*255);
color(r) = icolor(r) * 65536 + icolor(r) * 256 + 255;
plot '$infile' using 1:(\$2+$offset):($boxwidth):(color(\$3)) with boxes lc rgb variable notitle;
unset output;"

if [ -z "$verbose" ]
then
  printf "\nGnuplot script:\n%s\n\n" "$cmd"
else
  printf "Plotting timeseries histogram from '$folder' for '$field'"
  [ ! -z "$outfile" ] && printf " into '$outfile'"
  printf "\n"
fi

gnuplot -e "$cmd" --persist $persist

if [ "$outfile" ]
then
  echo "Generated $outfile"
fi
