#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Illegal number of parameters; Please provide unique run identifier as the parameter;"
    exit 1
fi

st=1
end=5
run=$1

for i in `seq 1 10`; do
	nohup ./pull_diff_files_and_xfsmonkey_log.sh $run $st $end > out$i.log &
	st=`expr $st + 5`
	end=`expr $st + 5`
done
