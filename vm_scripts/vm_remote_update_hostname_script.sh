#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Illegal number of parameters; Please provide vm number as the parameter;"
    exit 1
fi

num=$1
hostname='ubuntu16-vm'$num

echo $hostname > /etc/hostname
sed -i -e 's/ubuntu16-vm1/'$hostname'/g' /etc/hosts
sudo hostname $hostname

