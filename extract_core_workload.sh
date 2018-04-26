#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Illegal number of parameters; Please provide path to workload file as the parameter;"
    exit 1
fi

file=$1

cat $file | grep -v user_tools | grep -e link -e unlink -e mkdir -e sync -e Sync -e Rename -e WriteData -e Open -e 'checkpoint +=' -e FALLOC
