#!/bin/bash


if [ "$#" -ne 2 ]; then
    echo "Illegal number of parameters; Please provide batch size and num_servers as the parameters;"
    exit 1
fi

batch_size=$1
st=1
end=`expr $st + $batch_size - 1`
num_servers=$2

i=1
while [ $st -le $num_servers ]; do
	nohup ./scp_segregated_workloads.sh $st $end > out"$i".log &
	st=`expr $st + $batch_size`
	end=`expr $end + $batch_size`
	i=`expr $i + 1`
done
