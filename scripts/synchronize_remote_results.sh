#!/bin/bash

if [ $# -ne 2 ]
then
  echo "Usage: $0 <remote-folder> <local-folder>"
  echo "       Synchronizes final data from <remote-folder> into <local-folder>"
  exit 10
fi

remoteFolder=$(readlink -f $1)
if [ ! -d "$remoteFolder" ]
then
  echo "ERROR: remote result folder '$1' does not exist. Aborting"
  exit 1
fi

localFolder=$(readlink -f $2)
if [ ! -d "$localFolder" ]
then
  echo "ERROR: local result folder '$2' does not exist. Aborting"
  exit 2
fi

remotePath=${1#olympe/}
regularFiles=$(ssh olympe "cd code/ReusWorld/tmp/$remotePath; find -L . -name 'phylogeny.ptree.json' -o -name 'global.dat' -o -name '*.plant.json' -o -name '*.env.json' -o -name 'log' -o -regex '.*configs/[^/]*.config'")
lastSaves=$(ssh olympe "cd code/ReusWorld/tmp/$remotePath; find . -name 'run_*' | xargs -I {} bash -c 'find {} -name \"*.save.ubjson\" -printf \"%T@ %p\n\" | sort -n | cut -d \" \" -f 2- | tail -n 1'")
echo $regularFiles $lastSaves | tr " " "\n" | sort | rsync -avh --stats --info=progress2 --files-from=- $remoteFolder/ $localFolder
