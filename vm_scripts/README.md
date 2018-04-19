## Creating new instances
### Getting the instances up and running on chameleon
1. Go to http://chi.tacc.chameleoncloud.org/.  
2. You might need to create a new lease max instances are already created for existing leases.  
3. Lease option will be under 'Reservations' tab on the left, create a new lease with some min nodes and max nodes with the filters as node_type: Compute_haswell and local_gb >= 120 (since we will need to run more VMs and each VM will need 10 GB).  
4. Once the lease is created, go to Instances page by clicking on 'Compute' tab on the left. 
5. Click on launch instance and do the following -  
  a. Give some instance name, select the appropriate lease and give the number of nodes you want to create.  
  b. Select 'CC-Ubuntu16.04' as the Source.  
  c. Select 'baremetal' as the flavor.  
  d. Go to Key Pair and select 'crashmonkey'.  
  e. Select 'Launch instance'. 
6. If you have enough hosts, then the instances will be launched (it will take around 10-15 minutes for the initial build process).  
7. You need to associate a floating IP to each of the instances created (you can manage the floating IPs through 'Network' tab on the left).  
8. Once all this is done, you should be able to ping the machine using the floating IP.  
9. To ssh into the machines, you need the private key (ask Pandian for the key - crashmonkey.pem).  
   `ssh -i <path_to_pem_file> cc@<floating_ip>`
10. In order to be able to cssh to the nodes, add the IdentityFile config to your ~/.ssh/config file (check the mail that I sent with the subject 'Chameleon VMs are up and running').  

### Setting up VMs on the instances
1. Once the instances are up, make sure ssh into all the instances work (sometimes it doesn't work, in which case you might want to recreate the instance).  
2. Double check if the instances have enough disk space (> 200 GB).  
3. Copy all the scripts under vm_scripts folder of crashmonkey git repo to the home directory of all the instances (you will need all the scripts).  
4. Install the latest kernel by executing `./install-4.16.sh` followed by `sudo reboot`. In a while, you will be able to ssh again.  
5. Get a copy of `ubuntu16-vm1-export.ova` file in ~/ in the instances (which is the exported version of the VMs from other machines). This file should be obtainable from one of our own servers (chennai/thoothukudi/tenali/udipi/erode - for example, using `scp pandian@chennai.csres.utexas.edu:~/ubuntu16-vm1-export.ova ~/`.  
5. Now in all the instances, run `./setup.sh`, which sets up crashmonkey, installs virtual box and imports the VMs.  
6. If at any point setup script fails (which is more likely to happen), please resume the steps from which point it failed. You should be able to figure out why it failed by looking at the setup script.   
7. At this point, all VMs are set up and are already started and running. You might want to run `ssh-keygen` in the chameleon instances and copy the ~/.ssh/id_rsa.pub to the VMs' ~/.ssh/authorized_keys (using `scp -P <port> ~/.ssh/id_rsa.pub /home/pandian/.ssh/authorized_keys`) so that logging into the VMs will be easier (won't ask for password).  
