#!/bin/bash

if [ $# -ne 3 ]
then
  echo "Usage: $0 <outfile> <local-dependencies-folder> <other-dependencies-folder>"
  exit 10
fi

extractfield(){
  grep $1 $2.dependency | cut -d ' ' -f 2-
}

me=$2
them=$3
out=$1
echo "/*! \brief Auto-generated file do not modify */" > $out
echo "#include \"dependencies.h\"" >> $out
echo >> $out

echo "namespace config {" >> $out
echo "#define CFILE Dependencies" >> $out
for dep in $them/cxxopts $them/json $them/Tools $them/apt $me/ReusWorld
do
  proj=$(basename $dep)
  proj=${proj,}
  for field in buildDate commitHash commitMsg commitDate
  do
    value=$(extractfield $field $dep)
    [ ! -z "$value" ] && echo "DEFINE_CONST_PARAMETER(std::string, $proj${field^}, \"$value\")" >> $out
  done
done
echo "}" >> $out
