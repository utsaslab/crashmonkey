#!/bin/bash

# update the aliases
# shortcut aliases to start, stop or ssh into a vm
cat vm_aliases >> ~/.bashrc

# to source the aliases we just added
source ~/.bashrc

# set up crashmonkey in this vm
mkdir projects
cd projects/

sudo apt-get update
sudo apt-get install -y git
sudo apt-get install -y sshpass # to enter password for ssh non-interactively

git clone https://github.com/utsaslab/crashmonkey
sudo apt-get install -y btrfs-tools f2fs-tools xfsprogs libelf-dev

cd ~/

# install virtual box
mkdir virtualbox
cd virtualbox/

wget https://download.virtualbox.org/virtualbox/5.2.8/virtualbox-5.2_5.2.8-121009~Ubuntu~xenial_amd64.deb
wget https://download.virtualbox.org/virtualbox/5.2.8/Oracle_VM_VirtualBox_Extension_Pack-5.2.8-121009.vbox-extpack

#sudo apt-get install linux-headers-generic linux-headers-4.4.0-116-generic
sudo apt-get install -y dkms build-essential linux-headers-generic linux-headers-`uname -r`

sudo dpkg -i virtualbox*.deb
sudo apt-get install -y -f

#sudo apt install virtualbox*.deb
sudo VBoxManage extpack install --replace Oracle_VM_VirtualBox_Extension_Pack-5.2.8-121009.vbox-extpack

# import existing 1 VM
VBoxManage import ~/ubuntu16-vm1-export.ova

cd ~/

# Cloning the 1 VM to 15 more VMs
./clone_vms.sh 2 16 3023

export num_vms=`cat ~/.bashrc | grep num_vms | cut -d '=' -f2`

# Start all the vms
./start_all_vms.sh

# Wait for a minute for all VMs to start
echo 'Sleeping for a minute...'
sleep 60

# SCP all the remote scripts (scripts that reside on the VMs that we trigger remotely from the chameleon instances using rsh) to the VMs
./scp_remote_scripts_to_vms.sh

# Update the host names in the VMs - since we cloned the VM, all VMs will have the same hostname as ubuntu16-vm1
./trigger_remote_script_update_hostname.sh

# now we are done
echo 'Server and VMs setup complete (hopefully) :)'
