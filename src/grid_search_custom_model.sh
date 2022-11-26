#!/bin/bash

while [ $# -gt 0 ]; do
    if [[ $1 == "--"* ]]; then
        v="${1/--/}"
        declare "$v"="$2"
        shift
    fi
    shift
done

ghistoryBitsFrom=${ghistoryBitsFrom:-4}
ghistoryBitsTo=${ghistoryBitsTo:-31}
ghistoryBitsStep=${ghistoryBitsStep:-1}
pcIndexBitsFrom=${pcIndexBitsFrom:-4}
pcIndexBitsTo=${pcIndexBitsTo:-16}
pcIndexBitsStep=${pcIndexBitsStep:-1}
trainingThresholdBitsFrom=${trainingThresholdBitsFrom:-4}
trainingThresholdBitsTo=${trainingThresholdBitsTo:-16}
trainingThresholdBitsStep=${trainingThresholdBitsStep:-1}
weightsBitsFrom=${weightsBitsFrom:-4}
weightsBitsTo=${weightsBitsTo:-12}
weightsBitsStep=${weightsBitsStep:-1}
outputCsv=${outputCsv:-../grid_search}
outputAverageCsv="../"$outputCsv"_average.csv"
outputCsv="../"$outputCsv".csv"
skipBadModels=${skipBadModels:-true}

# usage
# bash grid_search_custom_model.sh --ghistoryBitsFrom 16 --ghistoryBitsTo 24 --ghistoryBitsStep 2 --pcIndexBitsFrom 4 --pcIndexBitsTo 12 --pcIndexBitsStep 2 --trainingThresholdBitsFrom 4 --trainingThresholdBitsTo 16 --trainingThresholdBitsStep 2 --weightsBitsFrom 4 --weightsBitsTo 12 --weightsBitsStep 2

compute_predictor_size() {
    local ghistoryBits=$1
    local pcIndexBits=$2
    local weightsBits=$3
    local size=$(( (ghistoryBits + 1) * 2**pcIndexBits * (weightsBits + 1) ))
    echo $size
}

echo "trace_ghistoryBits,pcIndexBits,trainingThresholdBits,weightsBits,size,mis_prediction_rate,better_than_tournament" > $outputCsv
echo "ghistoryBits,pcIndexBits,trainingThresholdBits,weightsBits,size,avg_mis_prediction_rate,better_than_tournament" > $outputAverageCsv

make clean > /dev/null 2>&1
make > /dev/null 2>&1

traces=('mm_1.bz2' 'mm_2.bz2' 'fp_1.bz2' 'fp_2.bz2' 'int_1.bz2' 'int_2.bz2') # 6 traces

# tournament predictor results
tournamentResults=()
for trace_index in "${!traces[@]}"; do
    trace=${traces[$trace_index]}
    read tournament_mp <<< $(bunzip2 -kc ../traces/$trace | ./predictor --tournament:9:10:10 | awk '/Misprediction Rate/ {print $3}')
    tournamentResults+=($tournament_mp)
done
# echo "Tournament results: ${tournamentResults[@]}"

total_models=$(( ((ghistoryBitsTo - ghistoryBitsFrom) / ghistoryBitsStep + 1) * ((pcIndexBitsTo - pcIndexBitsFrom) / pcIndexBitsStep + 1) * ((trainingThresholdBitsTo - trainingThresholdBitsFrom) / trainingThresholdBitsStep + 1) * ((weightsBitsTo - weightsBitsFrom) / weightsBitsStep + 1) ))
echo "Total number of models: $total_models"

model_index=0
start_time=$(date +%s)

for (( ghistoryBits=ghistoryBitsFrom; ghistoryBits<=ghistoryBitsTo; ghistoryBits = ghistoryBits + ghistoryBitsStep )); do
    for (( pcIndexBits=pcIndexBitsFrom; pcIndexBits<=pcIndexBitsTo; pcIndexBits = pcIndexBits + pcIndexBitsStep )); do
        for (( trainingThresholdBits=trainingThresholdBitsFrom; trainingThresholdBits<=trainingThresholdBitsTo; trainingThresholdBits = trainingThresholdBits + trainingThresholdBitsStep )); do
            for (( weightsBits=weightsBitsFrom; weightsBits<=weightsBitsTo; weightsBits = weightsBits + weightsBitsStep )); do
                size=$(compute_predictor_size $ghistoryBits $pcIndexBits $weightsBits)
                if [ $size -le 64576 ]; then
                    # echo "Evaluating ghistoryBits=$ghistoryBits, pcIndexBits=$pcIndexBits, trainingThresholdBits=$trainingThresholdBits, weightsBits=$weightsBits"
                    sed -i -e "s/CUSTOM_GHISTORY_BITS.*/CUSTOM_GHISTORY_BITS $ghistoryBits/g" predictor.h
                    sed -i -e "s/CUSTOM_PC_INDEX_BITS.*/CUSTOM_PC_INDEX_BITS $pcIndexBits/g" predictor.h
                    sed -i -e "s/CUSTOM_TRAINING_THRESHOLD_BITS.*/CUSTOM_TRAINING_THRESHOLD_BITS $trainingThresholdBits/g" predictor.h
                    sed -i -e "s/CUSTOM_WEIGHTS_BITS.*/CUSTOM_WEIGHTS_BITS $weightsBits/g" predictor.h

                    make clean > /dev/null 2>&1
                    make > /dev/null 2>&1

                    sum_mis_prediction_rate=0
                    better_than_tournament=1
                    total_branches=0
                    total_mispredictions=0
                    for trace_index in "${!traces[@]}"; do
                        trace=${traces[$trace_index]}
                        curr_better_than_tournament=1
                        output=`bunzip2 -kc ../traces/$trace | ./predictor --custom`
                        read custom_mis_prediction <<< $(echo "$output" | awk '/Misprediction Rate/ {print $3}')
                        read number_of_branches <<< $(echo "$output" | awk '/Branches/ {print $2}')
                        read number_of_mispredictions <<< $(echo "$output" | awk '/Incorrect/ {print $2}')

                        total_branches=$((total_branches + number_of_branches))
                        total_mispredictions=$((total_mispredictions + number_of_mispredictions))

                        tournament_mis_prediction=${tournamentResults[$trace_index]}
                        if [ $(echo "$custom_mis_prediction > $tournament_mis_prediction" | bc -l) -eq 1 ]; then
                            curr_better_than_tournament=0
                            better_than_tournament=0
                        fi
                        echo "$trace,$ghistoryBits,$pcIndexBits,$trainingThresholdBits,$weightsBits,$size,$custom_mis_prediction,$curr_better_than_tournament" >> $outputCsv
                        if [ $skipBadModels == true ] && [ $curr_better_than_tournament == 0 ]; then
                            break
                        fi
                    done
                    avg_mis_prediction_rate=$(echo "scale=4; $total_mispredictions * 100 / $total_branches" | bc -l)

                    # only log if better than tournament or if skipBadModels is false
                    if [ $skipBadModels == false ] || [ $better_than_tournament == 1 ]; then
                        echo "$ghistoryBits,$pcIndexBits,$trainingThresholdBits,$weightsBits,$size,$avg_mis_prediction_rate,$better_than_tournament" >> $outputAverageCsv
                    fi
                fi

                model_index=$((model_index + 1))

                # print process every 10 models
                if [ $((model_index % 10)) -eq 0 ]; then
                    current_time=$(date +%s)
                    time_elapsed=$((current_time - start_time))
                    fraction_models_completed=$(echo "scale=4; $model_index / $total_models" | bc -l)
                    estimated_time_remaining=$(echo "scale=0; $time_elapsed / $fraction_models_completed - $time_elapsed" | bc -l)
                    mins_remaining=$(echo "scale=0; $estimated_time_remaining / 60" | bc -l)
                    seconds_remaining=$(echo "scale=0; $estimated_time_remaining % 60" | bc -l)
                    estimated_time_in_mins_and_seconds=$(printf "%02d:%02d" $mins_remaining $seconds_remaining)
                    echo "Processed $(echo "scale=2; $fraction_models_completed * 100" | bc -l)% of models. Estimated time remaining: $estimated_time_in_mins_and_seconds"
                fi
            done
        done
    done
done