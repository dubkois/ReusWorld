#!/bin/bash

if [ $# -lt 5 ]
then
  echo "Usage: $0 <build-type> <res-folder>  <config-folder> <environment> <plant><other simulator arguments...>"
  echo "       Performs a run with build type <build-type> inside <res-folder>"
  exit 10
fi

buildFolder=$(readlink -e "./build_$1")
if [ ! -d "$buildFolder" ]
then
  echo "ERROR: build folder '$buildFolder' does not exist. Aborting"
  exit 1
fi

resFolder=$(readlink -m $2)
if [ -d "$resFolder" ]
then
  echo "ERROR: result folder '$resFolder' already exists. Aborting"
  exit 2
fi

configFolder=$(readlink -m $3)
if [ ! -d "$configFolder" ]
then
  echo "ERROR: config folder '$configFolder' does not exist. Aborting"
  exit 3
fi

envFile=$(readlink -e $4)
if [ ! -f "$envFile" ]
then
  echo "ERROR: Environment genome file '$envFile' does not exist. Aborting"
  exit 4
fi

plantFile=$(readlink -e $5)
if [ ! -f "$plantFile" ]
then
  echo "ERROR: Plant genome file '$envFile' does not exist. Aborting"
  exit 5
fi

shift 5

cmd="$buildFolder/simulator -e $envFile -p $plantFile $@ | tee -a log "
mkdir -pv $resFolder \
  && cd $resFolder \
  && cp -rv $configFolder $resFolder/configs \
  && echo "Executing '$cmd' from $resFolder" | tee log \
  && echo "Starting at $(date)" | tee -a log \
  && eval $cmd \
  && echo "Completed at $(date)" | tee -a log
