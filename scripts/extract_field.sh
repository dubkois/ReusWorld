#!/bin/bash

if [ $# -ne 4 ]
then
  echo "Usage: $0 <field> <folder> <bandwidth> <additional-command>"
  echo "       Extract <field> values for all experiments under <folder>"
  exit 10
fi

if [ -z "$1" ]
then
  echo "ERROR: '$1' is not a field. Aborting"
  exit 1
fi
field=$1

if [ ! -d "$2" ]
then
  echo "ERROR: '$2' does not exist. Aborting"
  exit 2
fi
folder=$2

process_one_file(){
  file=$1
  id=$(echo $file | sed 's|.*/completed/\(.*\)/run_\(.*\)/autosaves.*|\1_\2|')
  ./build_release/analyzer -l $file --extract-field=$field | grep $field | sed "s/$field: \(.*\)/$id \1/"
}
export -f process_one_file
export field

echo "Extracting and plotting $field"
analysisFolder="$folder/_analysis/"
mkdir -p $analysisFolder
for exp in $folder/*/
do
  if ! [[ "$(basename $exp)" == _* ]]
  then
    echo "Processing $exp"
    outfile=$analysisFolder/$field.$(basename $exp).extraction
    [ ! -f $outfile ] && find $exp -name "y100.save.ubjson" | xargs -I {} bash -c 'process_one_file {}' > $outfile 
    imgfile=$outfile.pdf
    [ ! -f $imgfile ] && gnuplot -c ./scripts/violin_plot.gp $outfile $3 $imgfile "set ylabel '$field ($(basename $exp))';$4"
  fi
done

echo "Processing $folder/global"
outfile=$analysisFolder/$field.global.extraction
[ ! -f $outfile ] && find $analysisFolder -name "$field.*.extraction" | grep -v "$field.global.extraction" | xargs -I {} bash -c 'exp=$(echo {} | sed "s/.*\.\(.*\).extraction/\1/"); awk -v e=$exp "{\$1=e; print}" {}' > $outfile

imgfile=$outfile.pdf
[ ! -f $imgfile ] && gnuplot -c ./scripts/violin_plot.gp $outfile $3 $imgfile "set ylabel '$field (global)';$4"


