#!/bin/bash

# set up crashmonkey in this vm
mkdir projects
cd projects/

sudo apt-get update
sudo apt-get install git

git clone https://github.com/utsaslab/crashmonkey
sudo apt-get install btrfs-tools f2fs-tools xfsprogs libelf-dev

cd ~/

# install virtual box
mkdir virtualbox
cd virtualbox/

wget https://download.virtualbox.org/virtualbox/5.2.8/virtualbox-5.2_5.2.8-121009~Ubuntu~xenial_amd64.deb
wget https://download.virtualbox.org/virtualbox/5.2.8/Oracle_VM_VirtualBox_Extension_Pack-5.2.8-121009.vbox-extpack

#sudo apt-get install linux-headers-generic linux-headers-4.4.0-116-generic
sudo apt-get install dkms build-essential linux-headers-generic linux-headers-`uname -r`

sudo dpkg -i virtualbox*.deb
sudo apt-get install -f

#sudo apt install virtualbox*.deb
sudo VBoxManage extpack install --replace Oracle_VM_VirtualBox_Extension_Pack-5.2.8-121009.vbox-extpack

# import existing 4 VMs
VBoxManage import ~/ubuntu16-4vms-export.ova

