#!/bin/bash

if [ "$#" -ne 3 ]; then
    echo "Illegal number of parameters; Please provide start vm number, end vm number and starting port as the parameters;"
    exit 1
fi

start=$1
end=$2
port=$3

for i in `seq $start $end`; do
	echo `date` Cloning to VM $i...
	VBoxManage clonevm ubuntu16-vm1 --name ubuntu16-vm"$i" --register
	VBoxManage modifyvm ubuntu16-vm"$i" --natpf1 delete ssh
	VBoxManage modifyvm ubuntu16-vm"$i" --natpf1 "ssh,tcp,,"$port",,22"
	port=`expr $port + 1`
done
