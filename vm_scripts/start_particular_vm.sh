#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Illegal number of parameters; Please provide vm number as the parameter;"
    exit 1
fi
vm=$1

echo Starting VM $vm...
VBoxManage startvm ubuntu16-vm"$vm" --type headless

