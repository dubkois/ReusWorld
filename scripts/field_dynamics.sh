#!/bin/bash

show_help(){
  echo "Usage: $0 -f <base-folder> -e <field-expression> [-b <bins>] [-p] [-a <cmd>]"
  echo "       -f <base-folder> the folder under which to search for *.save.ubjson"
  echo "       -e <field-expression> a json-like expression (see jq-1.5)"
  echo "       -b <bins> number of bins for each histogram"
  echo "       -p persist"
  echo "       -o <file> output file (implies no loop and no persist)"
  echo "       -t <title> Add a title for the plot"
  echo "       -a <cmd> Additional processing on the field output"
  echo "       -c Remove formatted datafiles before exiting (default)"
  echo "       -n Do not remove formatted datafile before exiting"
  echo
  echo "Post-processing examples:"
  echo
  echo "  -e \".morphology\" -a \"grep shoot | cut -d ':' -f 2 | grep -v S | awk '{ print gsub(/f/, \"\") }'\""
  echo "  Plots the evolution of the number of flowers in the germinated phenotypes"
  echo
  echo "  -e \".boundingBox\" -a \"grep \"^.boundingBox\" | cut -d ':' -f 2 | sed 's/ { {\(.*\),\(.*\)}, {\(.*\),\(.*\)} }/\1 \2 \3 \4/' | awk '{ print (\$2 - \$4) }'"
  echo "  Plots the evolution of height in the phenotypes"
}

datafile(){
  echo "$folder/field_dynamics$field.$1"
}

[ -z "$BUILD_DIR" ] && BUILD_DIR=./build_release
analyzer=$BUILD_DIR/analyzer
if [ ! -f $analyzer ]
then
  echo "Unable to find analyzer program at '$analyzer'."
  echo "Make sure you specified BUILD_DIR correctly and retry"
  exit 1
fi

# A POSIX variable
OPTIND=1         # Reset in case getopts has been used previously in the shell.

folder=""
field=""
bins="15"
persist=""
loop=""
outfile=""
verbose=""
v="" #"-v"
postprocessing="cut -d ':' -f 2"
clean="yes"
title=""
while getopts "h?e:f:b:po:a:t:qcn" opt; do
    case "$opt" in
    h|\?)
        show_help
        exit 0
        ;;
    e)  field=$OPTARG
        ;;
    f)  folder=$OPTARG
        ;;
    b)  bins=$OPTARG
        ;;
    p)  persist="-"
        ;;
    a)  postprocessing=$OPTARG
        ;;
    o)  outfile=$OPTARG
        ;;
    t)  title=$OPTARG
        ;;
    q)  verbose=no
        v=""
        ;;
    c)  clean="yes"
        ;;
    n)  clean="no"
        ;;
    esac
done

if [ -z "$verbose" ]
then
  echo "Using following analyzer:"
  ls -l $analyzer
fi

if [ "$outfile" ]
then
  [ "$outfile" == "auto" ] && outfile=$(datafile "png")
  output="set term pngcairo size 1680,1050;
set output '$outfile';
"
  persist=""
  loop=""
fi

# Explore folder (while following symbolic links) to find relevant saves
files=$(find -L $folder -path "*_r/*.save.ubjson" | sort -V)
if [ -z "$verbose" ]
then
  echo "folder: '$folder'"
  echo " field: '$field'"
  echo "  bins: '$bins'"
  echo
  echo "Found $(wc -l <<< "$files") save files"
fi

globalworkfile=$(datafile global)
for savefile in $files
do
  [ -z "$verbose" ] && printf "Processing $savefile\r"
  
  year=$(sed 's|.*y\([0-9][0-9]*\).save.ubjson|\1|' <<< $savefile)
  rawlocalworkfile=$(datafile "$year.raw")
  localworkfile=$(datafile "$year.local")
  
  if [ -f $localworkfile ]
  then
    [ -z "$verbose" ] && printf "Skipping alreay processed $savefile\r"
  
  else
    $analyzer -l $savefile --load-constraints none --extract-field $field | grep "^$field" > $rawlocalworkfile

    cat $rawlocalworkfile | eval "$postprocessing" > $localworkfile
   
    [ "$clean" == "yes" ] && rm $v $rawlocalworkfile
    
    if [ ! -f $localworkfile ] || [ $(wc -l $localworkfile | cut -d ' ' -f 1) -eq 0 ]
    then
      printf "Error processing $savefile\n"
    else
      stats=$(awk -v bins=$bins \
                  'NR==1 { min=$1; max=$1 }
                  NR>1 { if ($1 < min) min=$1; if (max < $1) max = $1 }
                  END { bsize=(max - min) / bins; print min, bsize, max }' $localworkfile)
      
      read min bsize max <<< "$stats"
      
#       echo
#       echo "$min, $bsize, $max"
#       awk -vmin=$min -v bsize=$bsize -v nbins=$bins 'BEGIN { for (i=0; i<nbins; i++) { print i*bsize+min, (i+.5)*bsize+min, (i+1)*bsize+min } }'
#       awk -vmin=$min -v bsize=$bsize -vmax=$max -v nbins=$bins -vyear=$year \
#              '{ b = ($1==max) ? nbins-1 : int(($1-min)/bsize); bins[b]++; }
#           END { for (i=nbins-1; i>=0; i--) { print year, bsize*(i+1)+min, bins[i] / NR }; print year, min, 0; }' $localworkfile
#       echo
      
      awk -vmin=$min -v bsize=$bsize -vmax=$max -vnbins=$bins -vyear=$year \
             '{ 
                if (min == max)   b = 0;
                else if ($1==max) b = nbins-1;
                else              b= int(($1-min)/bsize);
                bins[b]++;
              }
          END {
                if (min==max) {
                  minwidth=.1;
                  min -= .5*minwidth;
                  max += .5*minwidth;
                  print year, max, 1;
                  
                } else {
                  for (i=nbins-1; i>=0; i--) {
                    print year, bsize*(i+1)+min, bins[i] / NR
                  }
                }
                print year, min, 0;
              }' $localworkfile >> $globalworkfile
          
      [ -z "$verbose" ] && printf "Processed $savefile\r"
    fi
  fi

  [ "$clean" == "yes" ] && rm $v $localworkfile
done

[ -z "$verbose" ] && echo

globalmin=$(cut -d ' ' -f 2 $globalworkfile | LC_ALL=C sort -g | head -1)
[ -z "$verbose" ] && echo "global min: $globalmin"

globalmax=$(cut -d ' ' -f 2 $globalworkfile | LC_ALL=C sort -gr | head -1)
[ -z "$verbose" ] && echo "global max: $globalmax"

offset=0
if awk "BEGIN {exit !($globalmin < 0)}"
then
  offset=$(awk "BEGIN { print -1*$globalmin }" )
  [ -z "$verbose" ] && echo "Offsetting by $offset"
fi

cmd="set style fill solid 1 noborder;
set autoscale fix;" 

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
echo "ytics: $ytics"
  
cmd="$cmd
set format y \"%5.2g\";
set ytics ($ytics);"

if [ "$outfile" ]
then
  cmd="$cmd
$output"
fi

if [ "$title" ]
then
  cmd="$cmd
set title '$title';"
fi

cmd="$cmd
icolor(r) = int((1-r)*255);
color(r) = icolor(r) * 65536 + icolor(r) * 256 + 255;
plot '$globalworkfile' using 1:(\$2+$offset):(1):(color(\$3)) with boxes lc rgb variable notitle;"

if [ -z "$verbose" ]
then
  printf "%s\n" "$cmd"
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

[ "$clean" == "yes" ] && rm $v $globalworkfile
