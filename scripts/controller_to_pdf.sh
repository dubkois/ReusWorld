#!/bin/sh

usage(){
  echo "Usage: $0 <file.tex>"
}

if [ $# -ne 1 ]
then
  usage
  exit 10
fi

if [ ! -f "$1" ]
then
  usage
  exit 1
fi

file=$1
dir=$(dirname "$file")
base=$(basename "$file" .tex)
pdf=$base.pdf
png=${base}_tex

pdflatex -interaction batchmode "$file"
pdftocairo $pdf $png -png
mv -v $pdf $dir
mv -v $png-1.png $dir/$png.png
rm $base.aux $base.log 
