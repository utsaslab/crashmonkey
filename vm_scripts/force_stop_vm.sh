#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Illegal number of parameters; Please provide vm number as the parameter;"
    exit 1
fi

vm=$1
kill `ps aux | grep ubuntu16-vm"$vm" | grep -v grep | tr -s ' ' | cut -d ' ' -f2`

