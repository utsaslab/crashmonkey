#!/bin/bash

port=3030
for i in `seq 9 16`; do
	scp -P $port update_hostname.sh pandian@127.0.0.1:~/
	port=`expr $port + 1`
done
