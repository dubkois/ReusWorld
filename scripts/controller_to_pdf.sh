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

cd $dir
pdflatex -interaction batchmode "$base"
[ ! -f "$pdf" ] && exit 2

pdftocairo $pdf $png -png -scale-to 1680
mv $png-1.png $png.png
# rm $base.aux $base.log 
