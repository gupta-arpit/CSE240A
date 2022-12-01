#!/bin/bash

while [ $# -gt 0 ]; do
    if [[ $1 == "--"* ]]; then
        v="${1/--/}"
        declare "$v"="$2"
        shift
    fi
    shift
done

echo "Running make"
cd src; make clean; make

echo "trace,model,tournament:9:10:10,custom" > ../results.csv

for trace in $(ls ../traces); do
    echo "Evaluating $trace"
    read tournament_mis_prediction <<< $(bunzip2 -kc ../traces/$trace | ./predictor --tournament:9:10:10 | awk '/Misprediction Rate/ {print $3}')
    read custom_mis_prediction <<< $(bunzip2 -kc ../traces/$trace | ./predictor --custom | awk '/Misprediction Rate/ {print $3}')
    echo "$trace,$tournament_mis_prediction,$custom_mis_prediction" >> ../results.csv
done

cat ../results.csv