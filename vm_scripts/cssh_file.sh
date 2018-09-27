#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Illegal number of parameters; Please provide the file with list of IPs the parameters;"
    exit 1
fi

file=$1
hosts=`cat $file | tr '\r\n' ' '`
cssh -l cc $hosts
	
