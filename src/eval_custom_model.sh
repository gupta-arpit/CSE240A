#!/bin/bash

while [ $# -gt 0 ]; do
    if [[ $1 == "--"* ]]; then
        v="${1/--/}"
        declare "$v"="$2"
        shift
    fi
    shift
done

ghistoryBits=${ghistoryBits:-4}
pcIndexBits=${pcIndexBits:-4}
trainingThresholdBits=${trainingThresholdBits:-4}
weightsBits=${weightsBits:-4}

# usage
# bash eval_custom_model.sh --ghistoryBits 4 --pcIndexBits 4 --trainingThresholdBits 4 --weightsBits 4

compute_predictor_size() {
    local ghistoryBits=$1
    local pcIndexBits=$2
    local weightsBits=$3
    local size=$(( (ghistoryBits + 1) * 2**pcIndexBits * (weightsBits + 1) ))
    echo $size
}

size=$(compute_predictor_size $ghistoryBits $pcIndexBits $weightsBits)
if [ $size -gt 64576 ]; then
    continue
fi

echo "Evaluating ghistoryBits=$ghistoryBits, pcIndexBits=$pcIndexBits, trainingThresholdBits=$trainingThresholdBits, weightsBits=$weightsBits, size=$size"
sed -i -e "s/CUSTOM_GHISTORY_BITS.*/CUSTOM_GHISTORY_BITS $ghistoryBits/g" predictor.h
sed -i -e "s/CUSTOM_PC_INDEX_BITS.*/CUSTOM_PC_INDEX_BITS $pcIndexBits/g" predictor.h
sed -i -e "s/CUSTOM_TRAINING_THRESHOLD_BITS.*/CUSTOM_TRAINING_THRESHOLD_BITS $trainingThresholdBits/g" predictor.h
sed -i -e "s/CUSTOM_WEIGHTS_BITS.*/CUSTOM_WEIGHTS_BITS $weightsBits/g" predictor.h

make clean; make

for trace in $(ls ../traces); do
    read custom_mis_prediction <<< $(bunzip2 -kc ../traces/$trace | ./predictor --custom | awk '/Misprediction Rate/ {print $3}')
    read tournament_mp <<< $(bunzip2 -kc ../traces/$trace | ./predictor --tournament:9:10:10 | awk '/Misprediction Rate/ {print $3}')
    echo "$trace,Tournament: $tournament_mp, Custom: $custom_mis_prediction"
done
