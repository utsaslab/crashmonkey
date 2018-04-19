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
8. Make sure `live_nodes` file has the updated list of all the live chameleon instances we have (64 right now).  

## Triggering workloads in the VMs
### Getting the workloads
1. Given a list of cpp files, we need to evenly distribute them to all the VMs we have. Do a math of how many worklaods per VM by dividing total number of workloads by total number of VMs.  
2. Use the script `segregate_workloads.sh` to divide the workloads evenly into all the VMs. Run `./segregate_workloads.sh <num_per_vm> <max>` where `num_per_vm` is the approximate number of workloads per VM and `max` is the total number of workloads. Note that all the workload files will be of the format `j-lang<workload_num>.cpp`.  
3. Now, we need to get these segregated workloads to the chameleon instances. 
  * By default, these are scp'ed to ~/seq of the chameleon instances (hardcoded in the script) and you can run it as `./scp_segregated_workloads.sh <start_vm> <end_vm>` where `start_vm` and `end_vm` are just integers between 1 and number of live nodes we have (points to the corresponding line in the live_nodes file).  
  * You need to delete any files under ~/seq2 directory in all of the chameleon instances (by cssh'ing into all instances)  
  * This will scp the workloads serially which is slow, so use `./scp_segregated_workloads_parallel.sh` to trigger the scp in parallel (background processes).  
  * Note that the parallel script right now handles 64 nodes, if the number of live nodes changes, you need to change the parallel script as well.  
  * Also note that scp is not complete until `ps aux | grep scp_segregated` yields empty results. 
  
### Triggering the workloads
1. Now, we need to get the remote scripts and the workloads to the VMs from the chameleon instances. To do that, login into cssh into all chameleon instances, copy the latest vm_scripts/* (as checked in the repo) from somewhere (probably by scping from chennai machine). A sample command would be `for i in ``seq 1 5``; do sshpass -p alohomora scp pandian@chennai.csres.utexas.edu:~/projects/crashmonkey/vm_scripts/* .; echo Sleeping for 1 second..; sleep 1; done`.  
2. SCP all the remote scripts to the VMs using `./scp_remote_scripts_to_vms.sh`.  
3. SCP the workload files to the VMs using `./scp_workloads_to_vms.sh` which will scp the workload files to the corresponding VMs under `~/seq2` (you don't have to delete explicitly the folder in the VMs like you did for the chameleon instances).  
4. The final step is to trigger a workload using `./trigger_remote_script_trigger_xfsMonkey.sh <file_system>` which will trigger the `vm_remote_trigger_script.sh` in all the VMs. This will take some time (15-30 minutes) to complete.  

### Monitoring the workloads
Since all the workloads are running in the VMs, it is difficult to monitor the status of the runs. We have a few helper scripts for that as below -  
1. To check if the workload triggerd in the VMs is over or not, login to chameleon instances through cssh, pull latest copy of vm_scripts and run `./trigger_remote_script.sh vm_remote_check_running.sh` which either prints 'Some run in progress' or 'No runs in progress' for each of the VMs running.  
2. To check how many workloads are complete, cssh in to the chameleon instances and run `./trigger_remote_script.sh vm_remote_progress_script.sh` which will print how many workloads are complete for each of the VMs.  

### Getting the results
Once the workloads are complete (which you can make sure by using the above steps), you can pull the list of log files and the diff files as follows -  
1. Run `./pull_diff_files_and_xfsmonkey_log.sh <dir> <start_vm>	<end_vm>` which creates a directory dir and pulls the log files and diff files from the servers in the range [`start_vm`, `end_vm`].  
2. The above is a serial script which will take a long time. So, you can use `./pull_diff_files_and_xfsmonkey_log_parallel.sh <dir>` which pulls the necessary files from all the VMs. Similar to `scp_segregated_workloads_parallel.sh` script, you should change this parallel script also if the number of live nodes changes.  
3. Use `ps aux | grep pull_diff_files` to check if the pull script has completed. Once it's complete, you can find all the log and diff files under `dir` specified.  


