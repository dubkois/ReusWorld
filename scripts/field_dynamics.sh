#!/bin/bash
LC_ALL=C

extractorgans(){
  field=".morphology"
  layer="$1"
  postprocessing="grep \"$1\" | cut -d ':' -f 2 | grep -v S | awk '{ print gsub(/$2/, \"\") }'"
}

extractheight(){
  field=".boundingBox"
  postprocessing="cut -d ':' -f 2 | sed 's/ { {.*,\(.*\)}, {.*,\(.*\)} }/\1 \2/' | awk '{ print (\$1 - \$2) }'"
}

extractwidth(){
  field=".boundingBox"
  postprocessing="cut -d ':' -f 2 | sed 's/ { {\(.*\),.*}, {\(.*\),.*} }/\1 \2/' | awk '{ print (\$2 - \$1) }'"
}

declare -A fieldsArgs

fieldsCount=0

store(){
  store_i $fieldsCount "$1" "$2"
}

store_i(){
  fieldsArgs["${2}${1}"]="$3"
}

get(){
  echo "${fieldsArgs[$2$1]}"
}
  
show_help(){
  echo "Usage: $0 -f <base-folder> [OPTIONS]"
  echo
  echo "----------------------------------------------------------------------------------------------------"
  echo "Global options:"
  echo "       -f <base-folder> the folder under which to search for *.save.ubjson"
  echo "       -q Quiet"
  echo "       -v More verbose"
  echo "       -c Remove formatted datafiles before exiting (default)"
  echo "       -n Do not remove formatted datafile before exiting"
  echo 
  echo "----------------------------------------------------------------------------------------------------"
  echo "Plot-local options:"
  echo "       -e <field-expression> a json-like expression (see jq-1.5)"
  echo "       -b <bins> number of bins for each histogram"
  echo "       -o <file> output file (implies no loop and no persist)"
  echo "       -t <title> Add a title for the plot"
  echo "       --post-processing <cmd> Additional processing on the field output"
  echo "       -p Persist"
  echo "       -+ end of arguments list for current field"
  echo
  echo "Multiple commands can be provided (e.g. --field=.structuralLength -; --field=.cdata.optimalDistance"
  echo
  echo "----------------------------------------------------------------------------------------------------"
  echo "Post-processing examples:"
  echo
  
  extractorgans 'shoot' 'f'
  echo "  -e '$field' --layer '$layer' --postprocessing '$postprocessing'" 
  echo "  Plots the evolution of the number of flowers in the germinated phenotypes"
  echo "  Shortcut: --layer 'shoot' --organ 'f' (order is important)"
  echo

  extractheight 
  echo "  -e '$field' --postprocessing '$postprocessing'" 
  echo "  Plots the evolution of height in the phenotypes"
  echo "  Shortcut: --height"
  echo

  extractwidth
  echo "  -e '$field' --postprocessing '$postprocessing'" 
  echo "  Plots the evolution of width in the phenotypes"
  echo "  Shortcut: --width"
  echo
}

datafile(){
  [ -z "$3" ] && hidden=""
  echo "$folder/${hidden}field_dynamics$1.$2"
}

[ -z "$BUILD_DIR" ] && BUILD_DIR=./build_release
analyzer=$BUILD_DIR/analyzer
if [ ! -f $analyzer ]
then
  echo "Unable to find analyzer program at '$analyzer'."
  echo "Make sure you specified BUILD_DIR correctly and retry"
  exit 1
fi

folder=""
field=""
bins="15"
persist=""
outfile=""
verbose=""
superverbose="" #"-v"
eol="\r"
postprocessing="cut -d ':' -f 2"
clean="yes"
title=""

args=$(getopt --options "h?f:pqvcne:b:o:t:+" --longoptions "postprocessing:,layer:,organ:,width,height" -- "$@")
[ $? -eq 0 ] || exit 1

eval set --$args
[ $? -eq 0 ] || exit 2

while [[ $# -gt 0 ]]
do
  case "$1" in
  -h|[-]\?)
      show_help
      exit 0
      ;;
  -f) folder=$2; shift;
      ;;
  -q) verbose=no
      superverbose=""
      ;;
  -v) verbose=""
      superverbose="-v"
      eol="\n"
      ;;
  -c) clean="yes"
      ;;
  -n) clean="no"
      ;;

  -e) field=$2; shift;
      ;;
  -b) bins=$2; shift;
      ;;
  --postprocessing)
      postprocessing=$2; shift;
      ;;
  -o) outfile=$2; shift;
      ;;
  -t) title=$2; shift;
      ;;
  -p) persist="-"
      ;;
      
  --layer)
      layer=$2; shift
      ;;
      
  --organ)
      extractorgans $layer $2; shift
      ;;
      
  --height)
      extractheight
      ;;
      
  --width)
      extractwidth
      ;;
      
  -+|--)
      # Process fields arguments
      store field "$field"
      store bins "$bins"
      store pproc "$postprocessing"
      store outfile "$outfile"
      store term "$term"
      store title "$title"
      store persist "$persist"
      fieldsCount=$(($fieldsCount + 1))
      ;;
      
  *)  echo "Unrecognized option '$1'"
      show_help
      exit 3
      ;;
  esac
  shift
done

ifields=$(seq 0 $(($fieldsCount-1)))
fieldslist=""
for i in $ifields
do
  outfile=$(get $i outfile)
  if [ "$outfile" ]
  then
    field=$(get $i field)
    if [ "$outfile" == "auto" ]
    then
      outfile=$(datafile $field "png")
      store_i $i outfile $outfile
    fi
    
    store_i $i term "set term pngcairo size 1680,1050;"
    store_i $i output "set output '$outfile';"
    store_i $i persist ""
  fi
  
  fieldslist="${fieldslist}$(get $i field)\n"
done
fieldslist=$(printf "$fieldslist" | sed "s/\(.*\)/--extract-field=\1/" | paste -sd ' ')

if [ -z "$verbose" ]
then
  printf "%32s\n" ' ' | tr ' ' '-'
  printf "Arguments:\n"
  printf "%20s: %s\n" folder "$folder"
  printf "%20s: %s\n" verbose "$verbose"
  printf "%20s: %s\n" clean "$clean"
  printf "%20s: %s\n" v "$superverbose"
#   
#   for key in "${!fieldsArgs[@]}"; do
#     echo "$key: ${fieldsArgs[$key]}"
#   done
#   echo
#   
  for i in $ifields
  do
    printf "%32s\n" ' ' | tr ' ' '-'
    printf "Field $i\n"
    for f in field bins pproc outfile title output persist
    do
      v=$(get $i $f)
      [ ! -z "$v" ] && printf "%20s: %s\n" $f "$v"
    done
    printf "\n"
  done
  printf "%32s\n" ' ' | tr ' ' '-'
  echo
  echo "Using following analyzer:"
  ls -l $analyzer
  echo
fi

# Explore folder (while following symbolic links) to find relevant saves
files=$(find -L $folder -path "*_r/*.save.ubjson" | sort -V)
nfiles=$(wc -l <<< "$files")
if [ -z "$verbose" ]
then
  printf "%-80s$eol" "Found $nfiles save files"
fi

dyear=""

# First generate aggregated data for each field

i=1
for savefile in $files
do
  commonworkfile=$(datafile .common $i)
  if [ -f "$commonworkfile" ]
  then
    extract=no
    for f in $ifields
    do
      if [ $(grep -m 1 $(get $f field) $commonworkfile | wc -l) -eq 0 ]
      then
        extract=""
        break
      fi
    done
  fi

  if [ -z "$verbose" ]
  then
    if [ -z "$extract" ]
    then
      action="Process"
    else
      action="Skipp"
    fi
    
    printf "[%5.1f%%] %10s $savefile$eol" $((100 * $i / $nfiles)) ${action}ing
  fi

  year=$(sed 's|.*y\([0-9][0-9]*\).save.ubjson|\1|' <<< $savefile)
  [ -z "$dyear" ] && dyear=$year

  if [ -z "$extract" ]
  then
    $analyzer -l $savefile --load-constraints none --load-fields '!ptree' $fieldslist > $commonworkfile || exit 20
  fi
  
  for f in $ifields
  do
    field=$(get $f field)
    bins=$(get $f bins)
    
    localworkfile=$(datafile $field $i)
    globalworkfile=$(datafile $field "global")
    if [ ! -f "$localworkfile" ]
    then
      pproc=$(get $f pproc)
      cat $commonworkfile | grep "^$field" | eval "$pproc" > $localworkfile
    fi
    
    if [ ! -f $localworkfile ] || [ $(wc -l $localworkfile | cut -d ' ' -f 1) -eq 0 ]
    then
      printf "%-80s\n" "Error processing $localworkfile"
      exit 21
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
    fi

    [ "$clean" == "yes" ] && rm $superverbose $localworkfile
  done

  i=$(($i + 1))
  [ "$clean" == "yes" ] && rm $superverbose $commonworkfile
done

[ -z "$verbose" ] && printf "%-80s\n" "[100%%] Processed $(($i - 1)) savefiles"

# exit

for f in $ifields
do
  field=$(get $f field)
  outfile=$(get $f outfile)
  title=$(get $f title)
  persist=$(get $f persist) # Ignored
  
  globalworkfile=$(datafile $field global)
  
  $(dirname $0)/plot_field_dynamics.sh -f $field -i $globalworkfile -o $outfile -t $title
  
#   
#   printf "\n%80s\n" " " | tr ' ' '-'
#   printf "Generating graph for field $field\n"
#   
#   globalmin=$(cut -d ' ' -f 2 $globalworkfile | LC_ALL=C sort -g | head -1)
#   [ -z "$verbose" ] && echo "global min: $globalmin"
# 
#   globalmax=$(cut -d ' ' -f 2 $globalworkfile | LC_ALL=C sort -gr | head -1)
#   [ -z "$verbose" ] && echo "global max: $globalmax"
# 
#   offset=0
#   ytics=""
#   if awk "BEGIN {exit !($globalmin < 0)}"
#   then
#     offset=$(awk "BEGIN { print -1*$globalmin }" )
#     [ -z "$verbose" ] && echo "Offsetting by $offset"
#     
#     # Compute un-offsetted ytics
#     # Inspired by gnuplot/axis.c:678 quantize_normal_tics (yes it is kind of ugly. But! It works!!!)
#     ytics=$(awk -vmin=$globalmin -vmax=$globalmax -vtics=8 '
#       function floor (x) { return int(x); }
#       function ceil (x) { return int(x+1); }
#       BEGIN {
#         r=max-min;
#         power=10**floor(log(r)/log(10));
#         xnorm = r / power;
#         posns = 20 / xnorm;
#         
#         if (posns > 40)       tics=.05;
#         else if (posns > 20)  tics=.1;
#         else if (posns > 10)  tics=.2;
#         else if (posns > 4)   tics=.5;
#         else if (posns > 2)   tics=1;
#         else if (posns > .5)  tics=2;
#         else                  tics=ceil(xnorm);
#         
#         ticstep = tics * power;
#         
#         start=ticstep * floor(min / ticstep);
#         end=ticstep * ceil(max / ticstep);
#         
#         for (s=start; s<=end; s+=ticstep) {
#           if (s == 0) s = 0;  # To make sure zero is zero (remove negative zero)
#           printf "\"%g\" %g\n", s, s-min
#         }
#       }' | paste -sd,)
#     [ -z "$verbose" ] && echo "     ytics: $ytics"
#   fi
# 
#   boxwidth=$dyear
#   [ -z "$verbose" ] && echo "  boxwidth: $boxwidth"
# 
#   cmd="set style fill solid 1 noborder;
#   set autoscale fix;" 
# 
#   cmd="$cmd
#   set format y \"%5.2g\";
#   set ytics ($ytics);
#   set xlabel 'Years';
#   set ylabel '$field';"
# 
#   if [ "$outfile" ]
#   then
#     cmd="$cmd
#   $(get $f term)
#   $(get $f output)"
#   fi
# 
#   if [ "$title" ]
#   then
#     cmd="$cmd
#   set title '$title';"
#   fi
# 
#   cmd="$cmd
#   icolor(r) = int((1-r)*255);
#   color(r) = icolor(r) * 65536 + icolor(r) * 256 + 255;
#   plot '$globalworkfile' using 1:(\$2+$offset):($boxwidth):(color(\$3)) with boxes lc rgb variable notitle;"
# 
#   if [ -z "$verbose" ]
#   then
#     printf "\nGnuplot script:\n%s\n\n" "$cmd"
#   else
#     printf "Plotting timeseries histogram from '$folder' for '$field'"
#     [ ! -z "$outfile" ] && printf " into '$outfile'"
#     printf "\n"
#   fi
# 
#   gnuplot -e "$cmd" --persist $persist
# 
#   if [ "$outfile" ]
#   then
#     echo "Generated $outfile"
#   fi

  [ "$clean" == "yes" ] && rm $superverbose $globalworkfile
done
