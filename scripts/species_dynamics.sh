#!/bin/bash

if [ $# -ne 2 ]
then
  echo "Usage: $0 -f <folder> <n-champions>"
  exit 10
fi

folder=$1
if [ ! -d $folder ]
then
  echo "WARNING: '$folder' is not a valid folder. Aborting synchronisation"
else
  rsync -avh --stats --info=progress2 $folder/autosaves/ tmp/
fi

folder=tmp

printf "Generating species data\r"
for save in $folder/y*.save.ubjson
do
#   echo "
  if [ ! -f ${save%save.ubjson}ranges.dat ]
  then
    ./build_release/analyser -l $save --species-ranges
  fi
done

minYear=$(ls -v $folder/y*.save.ubjson | head -1 | sed 's/.*y\(.*\).save.ubjson/\1/')
maxYear=$(ls -vr $folder/y*.save.ubjson | head -1 | sed 's/.*y\(.*\).save.ubjson/\1/')

printf "Extracting Species list\r"
species=$(ls $folder/*ranges.dat | xargs -I {} cut -d ' ' -f 1 {} | grep -v "SID" | sort -g | uniq)
speciesCount=$(echo $species | wc -w)

total=$(awk '{ print $1 * ($3 - $2) }' <<< "$speciesCount $minYear $maxYear")
i=1

# set -x
outfile="tmp/species_dynamics.dat"
echo "$speciesCount Species" 
echo "[$minYear,$maxYear] years"
echo "$total total iterations"

printf "Extracting population count & ranges\r\n"
if [ ! -f $outfile ]
then
  printf "Year " > $outfile
  for s in $species
  do
    printf "${s}_{Pop} ${s}_{XMin} ${s}_{XMax} " >> $outfile
  done
  printf "\n" >> $outfile
  for y in $(seq $minYear $maxYear)
  do
    f="$folder/y$y.ranges.dat"
    printf "$y " >> $outfile
    for s in $species
    do
      step=$(awk '{ printf "%6.2f", 100*$1/$2}' <<< "$i $total") 
      printf "[$step%%] Processing y$y SID$s         \r" 
      i=$(expr $i + 1)
      
      res=$(grep "^$s " $f)
      if [ ! -z "$res" ]
      then
        awk '{ printf "%d %f %f ", $3, $2 - .5*$4, $2 + .5*$4 }' <<< "$res" >> $outfile
      else
        printf "nan nan nan " >> $outfile
      fi
    done
    printf "\n" >> $outfile
  done
  printf "Processed $(($maxYear - $minYear)) years and $speciesCount species ($total total items)\a\n"
fi

champcount=$2
champlist=tmp/.champlist
if [ ! -f $champlist ]
then
  awk -v NS=$speciesCount '
  function pop_i(i) {
    return 3*i-1
  }
  function pop_v(i) {
    return $(pop_i(i))
  }
  NR == 1 {
    for (i=1; i<=NS; i++) {
      split(pop_v(i),a,"_")
      sid[i] = a[1]
    }
  }
  NR > 1 {
    for (i=1; i<=NS; i++) if (pop_v(i) != "nan") sums[i] += pop_v(i)
  }
  END {
    for (i=1; i<=NS; i++)  printf "%s-%s %s %s\n", pop_i(i), pop_i(i)+2, sid[i], sums[i]
  }' $outfile | sort -t ' ' -k3gr | head -n $champcount > $champlist
fi

champspec="1"
for spec in $(cat $champlist | cut -d ' ' -f 1)
do
  champspec="$champspec,$spec"
done
# echo $champspec
rm $champlist

filteredFile=${outfile%.dat}.filtered.dat
cut -d ' ' -f $champspec $outfile | sed 's/\([^ ]*\)_{Pop} /\1 /g' > $filteredFile

rangesImg="${outfile%.dat}.ranges.pdf"
# if [ ! -f rangesImg ]
# then
  gnuplot -e "
  S=$champcount;
  F='$filteredFile';
  set terminal pdf size 6,4;
  set output '$rangesImg';
  set style fill transparent solid .1;
  set xlabel 'Year';
  set ylabel 'Range';
  set grid;
  plot for [s=1:S] F using 1:3*s:3*s+1 with filledcurves notitle,
       for [s=1:S] '' using 1:3*s with lines notitle,
       for [s=1:S] '' using 1:3*s+1 with lines notitle;"
# fi

popImg="${outfile%.dat}.pop.pdf"
if [ ! -f $popImg ]
then
  summedFiltered="${filteredFile%.filtered.dat}.filtered.summed.dat"
  awk -v NS=$champcount '
  function pop_i(i) {
    return 3*i-1
  }
  function pop_v(i) {
    return $(pop_i(i))
  }
  NR == 1 { print }
  NR > 1 {
    printf "%s 0 ", $1
    sum=0
    for (i=1; i<=NS; i++) {
      sum += pop_v(i)
      printf "%s ", pop_v(i) != "nan" ? sum : "nan"
    }
    printf "\n"
  }' $filteredFile > $summedFiltered

  gnuplot -e "
    S=$champcount;
    F='$summedFiltered';
    set terminal pngcairo size 1680,1050;
    set output '$popImg';
    set style fill transparent solid .1;
    notnan(s) = (stringcolumn(s) eq \"nan\") ? notnan(s-1) : column(s);
    plot for [s=1:S] F using 1:s+2:(notnan(s+1)) with filledcurves  notitle,
         for [s=1:S] F using 1:s+2 with lines notitle;"
fi
