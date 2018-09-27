# Chameleon Cloud Setup #

### Creating Instances ##
1. Create a new lease. Lease option will be under 'Reservations' tab on the left. Create a new lease with some min nodes and max nodes with the filters as node_type: Compute_haswell and local_gb >= 120 (since we will need to run more VMs and each VM will need 10 GB).  
2. Once the lease is created, go to Instances page by clicking on 'Compute' tab on the left.
3. Click on launch instance and do the following
  1.  Give some instance name, select the appropriate lease and give the number of nodes you want to create.  
  2. Select 'CC-Ubuntu16.04' as the Source.  
  3. Select 'baremetal' as the flavor.  
  4. Select a key-pair you created, if not create one and assign it.  
  5. Select 'Launch instance'.
5. If you have enough hosts, the instances will be launched (the initial build process takes around 10-15 minutes).  
6. You need to associate a floating IP to each of the instances created (you can manage the floating IPs through 'Network' tab on the left).  
7. Once all this is done, you should be able to ping the machine using the floating IP.  
8. To ssh into the machines, you need the private key, let's call it crashmonkey.pem  
   `ssh -i <path_to_pem_file> cc@<floating_ip>`
9. In order to be able to cssh to the nodes, add the IdentityFile config to your ~/.ssh/config file by adding the following line.
```
Host <floating_ip>
    IdentityFile /home/user/crashmonkey.pem
```

### Setting up VMs on the instances
1. Once the instances are up, make sure ssh into all the instances work (sometimes it doesn't work, in which case you might want to recreate the instance).  
2. Double check if the instances have enough disk space (> 200 GB).  
3. Copy all the scripts under vm_scripts folder of CrashMonkey git repo to the home directory of all the instances (you will need all the scripts).  
4. Install the latest kernel by executing `./install-4.16.sh` followed by `sudo reboot`. In a while, you will be able to ssh again.  
5. On a local server, create a VM and export it as `ubuntu16-vm1-export.ova`. Copy this exported VM to the root directory of all instances.
5. Now in all the instances, run `./setup.sh`, which sets up CrashMonkey, installs virtual box and imports the VMs.  
6. If at any point setup script fails (which is more likely to happen), please resume the steps from which point it failed. You should be able to figure out why it failed by looking at the setup script.   
7. At this point, all VMs are set up and are already started and running. You might want to run `ssh-keygen` in the chameleon instances and copy the ~/.ssh/id_rsa.pub to the VMs' ~/.ssh/authorized_keys (using `scp -P <port> ~/.ssh/id_rsa.pub /home/user/.ssh/authorized_keys`) so that logging into the VMs will be easier (won't ask for password).  
8. Make sure `live_nodes` file has the updated list of all the live Chameleon instances you have.  
