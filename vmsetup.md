### Setting up a new VM for CrashMonkey ###

If you are new to building and running VMs and would like to try something other than VirtualBox, I would recommend using `vmbuilder`, `libvert`, `qemu`, `kvm`, and the `vmbuilder` script in the repo to get everything setup (script generously provided by Ian Neal).

If you want to setup the VM via these scripts, following these steps:
1. Follow steps 1-3 [on this random website about setting up kvm on Ubuntu 16.04 LTS](https://www.cyberciti.biz/faq/installing-kvm-on-ubuntu-16-04-lts-server/)
1. To fix some odd permission issue with libvirt, run:
    1. `sudo apt-get install apparmor-profiles apparmor-utils`
    1. `sudo aa-complain /usr/lib/libvirt/virt-aa-helper`
1. `git clone` CrashMonkey repo into a directory of your choosing
1. edit `setup/create_vm.sh` to point to the directory you want the VM disk in, add any additional packages you may want, change user names
1. `setup/create_vm.sh <VM name> <VM IP>` to create a new VM and register it with `libvirt`
    1. Note that you may have to comment out line 153 in `/usr/lib/python2.7/dist-packages/VMBuilder/plugins/ubuntu/dapper.py` of `vmbuilder` python code in order to get it to run properly. Otherwise, it may have an issue with copying over sudo templates.
    1. Sit back and drink some coffee as this process may take a little while
1. `virsh edit <vm name>` and fix the disk that is passed into the VM as the
   boot drive. It defaults to the random alphanumeric name that `vmbuilder`
   generates, but the last few lines of the script moves it to the name of the
   VM itself.
    1. Note that you may also have to edit the name of the bridge by running
       `virsh edit <VM name>` depending on your system.
1. Fire up the newly created VM and `ssh` into it
