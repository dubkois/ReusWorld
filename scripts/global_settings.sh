#!/bin/sh

###############################################################################
## General

verbosity=0

###############################################################################
## Project 

project="ReusWorld"
projectPath="$HOME/thesis/code/$project"
build="release"

###############################################################################
## Connection to cluster (cecyle)

declare -A cecyle
cecyle[hostname]="cecyle"
cecyle[host]="${cecyle[hostname]}.irit.fr"
cecyle[user]="kevin"
cecyle[port]="3003"
cecyle[evos]="${cecyle[hostname]}:thesis/$project/evos/"
cecyle[localMount]="$projectPath/cecyle/"

declare -A eos
eos[hostname]="eos"
eos[host]="${eos[hostname]}.calmip.univ-toulouse.fr"
eos[user]="dubois"
eos[port]="22"
eos[evos]="${eos[hostname]}:$project/tmp/"
eos[localMount]="$projectPath/eos/"

declare -A olympe
olympe[hostname]="olympe"
olympe[host]="${olympe[hostname]}.calmip.univ-toulouse.fr"
olympe[user]="dubois"
olympe[port]="22"
olympe[evos]="${olympe[hostname]}:code/$project/tmp/"
olympe[localMount]="$projectPath/olympe/"

sshConfig="$HOME/.ssh/config"
login(){
  echo "$user@$host"
}

###############################################################################
## Error codes

E_WRONG_ARGUMENT_COUNT=1
E_WRONG_ARGUMENT_TYPE=2

E_CONNECTION_FAILED=10

E_CD_SCRIPT_FOLDER_FAILED=254
E_LOAD_CONFIG_SCRIPT=255

