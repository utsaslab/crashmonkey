#!/bin/bash

st=1
end=5

for i in `seq 1 10`; do
	nohup ./pull_diff_files_and_xfsmonkey_log.sh $st $end > out$i.log &
	st=`expr $st + 5`
	end=`expr $st + 5`
done
