#!/bin/bash

if [ "$#" -ne 3 ]; then
    echo "Illegal number of parameters; Please provide unique run identifier, batch size and num_servers as the parameter;"
    exit 1
fi

run=$1

batch_size=$2
st=1
end=`expr $st + $batch_size - 1`
num_servers=$3

i=1
while [ $st -le $num_servers ]; do
        nohup ./pull_diff_files_and_xfsmonkey_log.sh $run $st $end > out$i.log &
	st=`expr $st + $batch_size`
        end=`expr $end + $batch_size`
        i=`expr $i + 1`
done
