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
  echo "  -e \".morphology\" -a \"grep shoot | cut -d ':' -f 2 | awk '{ print gsub(/f/, \"\") }'\""
  echo "  Plots the evolution of the number of flowers in the phenotypes"
}

datafile(){
  echo "$folder/field_dynamics$field.$1"
}

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
    ./build_release/analyzer -l $savefile --load-constraints none --extract-field $field | grep "^$field" > $rawlocalworkfile

    cat $rawlocalworkfile | eval "$postprocessing" > $localworkfile
   
    [ "$clean" == "yes" ] && rm $v $rawlocalworkfile
    
    if [ ! -f $localworkfile ] || [ $(wc -l $localworkfile | cut -d ' ' -f 1) -eq 0 ]
    then
      printf "Error processing $savefile\n"
    else
      stats=$(awk -v bins=$bins \
                  'NR==1 { min=$1; max=$1 }
                  NR>1 { if ($1 < min) min=$1; if (max < $1) max = $1 }
                  END { if (min == max) { bsize=.1; hbins=int(.5*bins); min=min-hbins*bsize; max=max+hbins*bsize } else { bsize=(max - min) / bins }; print min, bsize, max }' $localworkfile)
      
      read min bsize max <<< "$stats"
      
#       echo "$min, $bsize, $max"
#       awk -vmin=$min -v bsize=$bsize -v nbins=$bins 'END { for (i=0; i<nbins; i++) { print (i-.5)*bsize+min, i*bsize+min, (i+.5)*bsize+min } }' <<< ""
      
      awk -vmin=$min -v bsize=$bsize -v nbins=$bins -v year=$year \
             '{ b = bsize * (int(($1-min)/bsize)+.5) + min; bins[b]++ }
          END { for (i=nbins-1; i>=0; i--) { b=bsize*(i+.5)+min; print year, b+.5*bsize, bins[b] / NR }; l=min-.5*bsize; if (l > 0) { print year, l, 0; } }' $localworkfile >> $globalworkfile
          
      [ -z "$verbose" ] && printf "Processed $savefile\r"
    fi
  fi

  [ "$clean" == "yes" ] && rm $v $localworkfile
done

cmd="set style fill solid 1 noborder;
set autoscale fix;" 

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
plot '$globalworkfile' using 1:2:(1):(color(\$3)) with boxes lc rgb variable fs solid 1 noborder notitle;"

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
