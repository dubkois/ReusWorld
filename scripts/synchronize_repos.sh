#!/bin/bash

usage(){
  echo "Usage: $0 <data-file> [pull|push]"
  echo
  echo
  echo "# Data-file"
  echo "contains rows repositories configuration formatted as follow"
  echo "Name;URL;Version;CMake options;Make options"
  echo
  echo "         Name: folder name for the cloned repository"
  echo "          URL: Repository location"
  echo "       Target: Branch or tag to checkout"
  echo "CMake options: Build setup options"
  echo " Make options: Compilation options (not covered by cmake)"
  echo 
  echo "# Dependencies"
  echo "If A > B (depends on) and C > (A, B) then the data-file should be ordered as"
  echo "B"
  echo "A"
  echo "C"
}

if [ $# -lt 1 -o $# -gt 2 ]
then
  usage
  exit 1
fi

now=$(LC_ALL=C date | tr " :" "_-")
log="reposync_log_$now"
echo "## $(LC_ALL=C date) ##" | tee $log
printf "Synchronizing repositiories (" | tee -a $log
if [ "$1" = "push" ]
then
  printf "pushing" | tee -a $log
else
  printf "pulling" | tee -a $log
fi
printf " changes)\n" | tee -a $log

ppath=$(readlink -m "./local/")
mkdir -p $ppath
echo "Using '$ppath' as local installs root" | tee -a $log

echo
wd=$(pwd)
while IFS=';' read name url target cmake make
do
  echo "Processing $name"
  
  if [ ! -d "$name" ]
  then
    git clone $url $name
  fi

  cd $name
  
  if [ "$1" != "push" ]
  then
    if [ "$target" != "-" ]
    then
      git fetch --tags
      git checkout $target
    else
      git pull
    fi  

    if [ "$cmake" != "-" ]
    then
      if [ ! -d "build_release" ]
      then
        echo ">> Setting up release build folder"
        mkdir build_release
        cd build_release
        cmake .. -DCMAKE_INSTALL_PREFIX=$ppath -DCMAKE_BUILD_TYPE=Release $cmake
      else
        cd build_release
      fi
      make $make
    fi
  else
    git commit -a
    git status
  fi
  
  cd $wd
  printf "Done.\n\n"

done < "$1" | tee -a $log

printf "All done\n" | tee -a $log
