#!/bin/bash

#include global settings
basefolder=$(dirname $(readlink -f "$0"))
source $basefolder/global_settings.sh || (echo "$0 unable to load config file in $basefolder"; exit 255);

usage(){
  echo "Usage: $0 <cecyle|olympe> [-d]"
  echo "       Mounts the evos folder of server '$1' onto the corresponding local mount"
  echo "        unless another sshfs instance is already running"
  echo "       With -d properly dismount the partition"
}

if [ $# -gt 2 ]
then
  usage
  exit $E_WRONG_ARGUMENT_COUNT
fi

if [ "$1" = "cecyle" ]
then
  declare -n server="cecyle"
elif [ "$1" = "olympe" ]
then
  declare -n server="olympe"
else
  echo "Unknown server '$1'" >&2
  usage
  exit $E_WRONG_ARGUMENT_TYPE
fi

echo "Server $1 data:"
for K in "${!server[@]}"; do printf "%15s: %-s\n" $K ${server[$K]}; done
echo

if [ "$2" = "-d" ]
then
  fusermount -u ${server[localMount]} && echo "Disconnected from ${server[evos]}"
else
  grep -q ${server[host]} $sshConfig
  if [ $? -eq 1 ]
  then
    configData="
    Host ${server[hostname]}\n
     HostName ${server[host]}\n
     User ${server[user]}\n
     Port ${server[port]}"
    
    echo
    echo -e $configData
    echo
    printf "Insert this in $sshConfig ?"
    
    read -n 1 res
    if [ "$res" == "y" ]
    then
      echo -e $configData >> $sshConfig
    fi
  fi

  # As-needed synchronization with remote folder
  if [ `pgrep -x "sshfs" -c` -eq 0 ]
  then
    echo "Synchronizing remote folder"

    maxAttempts=10
    attempts=0
    while [ `ls ${server[localMount]} | wc -l` -eq 0 ]
    do
      echo "sshfs ${server[evos]} ${server[localMount]} 2>&1 > sshfs.log" &
      sshfs ${server[evos]} ${server[localMount]} 2>&1 > sshfs.log &
      
      attempts=`expr $attempts + 1`
      if [ $attempts -gt $maxAttempts ]
      then
	echo "Connection to ${server[evos]} failed after $attempts attempts"
	echo "Try again at some other time"
	exit $E_CONNECTION_FAILED
      fi
      
      sleep 1
    done
  else
    echo "Found another instance of sshfs running. No action were performed"
  fi
fi
