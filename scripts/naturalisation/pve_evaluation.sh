#!/bin/bash

runs=$(ls -d results/timelines/data/*_*/)
nruns=$(wc -l <<< $runs)

echo "Found $nruns runs"

envs=$(ls results/timelines/data/environments/test_env_h*.json)
nenvs=$(wc -l <<< $envs)

echo "Found $nenvs envs"

total=$(($nruns * $nenvs))
echo "$total evaluations required"

processed=$(ls -d results/timelines/pve/*/ | wc -l)
echo "$processed already processed"

remaining=$(($total - $processed))
echo "$remaining remaining"

i=0

echo 

for r in $runs
do
  for te in $envs
  do
    odir=results/timelines/pve/$(basename $r)_$(sed 's/.*test_env_\(.*\).env.json/\1/' <<< $te)
    if [ ! -d $odir ]
    then
      LC_ALL=C printf "\033[32m[%05.1f%%]\033[0m Processing %s\n" $(echo "scale=2; 100 * $i / $remaining" | bc) $odir
      ./build_release/pve -f $odir --overwrite a --load-constraints none --lhs $(find $r -name "y1000.save.ubjson") \
                          --env $te --step 4 --stability-threshold 5e-3 --stability-steps 3
      i=$(($i + 1))
    fi
  done
done

LC_ALL=C printf "\033[32m[%05.1f%%]\033[0m Finished\n" $(echo "scale=2; 100 * $i / $remaining" | bc)
