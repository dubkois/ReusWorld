#!/bin/bash

usage(){
  echo "Usage: $0 [-f=<folder>] [-d=WxH] -o=file"
}


# A POSIX variable
OPTIND=1         # Reset in case getopts has been used previously in the shell.

folder="screenshots"
resolution="640x480"
outfile=""
while getopts "h?f:d:o:" opt; do
    case "$opt" in
    h|\?)
        usage
        exit 0
        ;;
    f)  folder=$OPTARG
        ;;
    f)  R=$(echo $1 | cut -d'=' -f 2)
        W=$(echo $R | cut -d'x' -f 1)
        H=$(echo $R | cut -d'x' -f 2)
        if [ -n "$W" -a -n "$H" -a "$W" -gt 0 -a "$H" -gt 0 ]
        then
          resolution=$R
        fi
        ;;
    o)  outfile=$OPTARG
        ;;
    esac
done

if [ "$outfile" == "" ]
then
  echo "No outfile defined!"
  usage
  exit 1
fi

ffmpeg -framerate 50 -i "$folder/step_%d.png" -s $resolution -an $outfile \
  && echo -e "\n\e[32m Sucessfully created video file $outfile \e[0m"

