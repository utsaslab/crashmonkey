# Deploying CrashMonkey on a Cluster


### Create instances
1. Get access to a cluster. On each machine, install as many VMs as it can support. Ensure at least 20GB of disk space on each VM, if you plan on running large tests. For our testing, we decided to use [Chameleon Cloud](http://chi.tacc.chameleoncloud.org/). We have also compiled the [instructions](chameleon.md) specific to setting up instances on the Chameleon Cloud.  
2. Ensure you have the private key to ssh to the created instances.
3. Additionally create a file called `live_nodes` and add the IP of all live instances you have.

### Triggering workloads in the VMs
### Segregating the workloads
1. Given a list of cpp files, we need to evenly distribute them to all the VMs we have. Do a math of how many workloads need to be copied over per VM by dividing the total number of workloads by the number of VMs you have.  
2. Use the script `segregate_workloads.sh` to divide the workloads evenly into all the VMs. Run `./segregate_workloads.sh <num_per_vm> <start_workload_number> <max> <path_to_workload_files> <output_workload_path>` where `num_per_vm` is the approximate number of workloads per VM, `max` is the total number of workloads, and you provide the path to the workloads to segregate along with the path to the directory where you want to place the segregated files. Note that all the workload files will be of the format `j-lang<workload_num>.cpp`.  What you'll see after running this script, is a bunch of subdirectories within the output directory, each with equal number of test files. For each VM `j` in the instance `i` with IP `ip`, you will find the corresponding workloads in the directory. `<output_workload_path>/<node<i>>-<ip>/vm<j>`

3. Now, we need to get these segregated workloads to the instances.
  * By default, these are scp'ed to ~/seq2 of the instances (hardcoded in the script) and you can run it as `./scp_segregated_workloads.sh <start_serv_num> <end_serv_num> <output_workload_path>` where `start_serv_num` and `end_serv_num` are just integers between 1 and number of live nodes we have (points to the corresponding line in the live_nodes file).  
  * You need to delete any files under ~/seq2 directory in all of the chameleon instances (by cssh'ing into all instances)  
  * This will scp the workloads serially which is slow, so use `./scp_segregated_workloads_parallel.sh` to trigger the scp in parallel (background processes).  
  * Note that the parallel script right now handles 64 nodes, if the number of live nodes changes, you need to change the parallel script as well.  
  * Also note that scp is not complete until `ps aux | grep scp_segregated` yields empty results.
  * If the instances had changed (recreated), then the entries have to be removed from ~/.ssh/known_hosts file otherwise the scp will fail with host verification failed. So, if the instances had changed, remove ~/.ssh/known_hosts and then trigger the pull script.  

### Triggering the workloads
1. Now, we need to get the remote scripts and the workloads to the VMs from the instances. To do that, cssh into all  instances, copy the latest vm_scripts/* (as checked in the repo) from a local server. Please set your username and password in all the following scripts.
2. SCP all the remote scripts to the VMs using `./scp_remote_scripts_to_vms.sh`.
3. SCP the workload files to the VMs using `./scp_workloads_to_vms.sh` which will scp the workload files to the corresponding VMs under `~/seq2` (you don't have to delete explicitly the folder in the VMs like you did for the chameleon instances).  
4. The final step is to trigger the workload using `./trigger_remote_script_trigger_xfsMonkey.sh <file_system>` which will trigger the `vm_remote_trigger_script.sh` in all the VMs. This will take some time (15-30 minutes) to complete.  

### Monitoring the workloads
Since all the workloads are running on the VMs, it is difficult to monitor the status of the runs. We have a few helper scripts for that.  
1. To check if the workload triggered in the VMs has completed execution, login to the instances through cssh, pull latest copy of vm_scripts and run `./trigger_remote_script.sh vm_remote_check_running.sh` which either prints 'Some run in progress' or 'No runs in progress' for each of the VMs running.  
2. To check how many workloads are complete, cssh in to the instances and run `./trigger_remote_script.sh vm_remote_progress_script.sh` which will print how many workloads are complete for each of the VMs.  

### Getting the results
Once the workloads are complete (which you can make sure by using the above steps), you can pull the list of log files and the diff files as follows -  
1. If the instances had changed (recreated), then the entries have to be removed from ~/.ssh/known_hosts file otherwise the scp will fail with host verification failed. So, if the instances had changed, remove ~/.ssh/known_hosts and then trigger the pull script.  
2. Run `./pull_diff_files_and_xfsmonkey_log.sh <dir> <start_vm>	<end_vm>` which creates a directory dir and pulls the log files and diff files from the servers in the range [`start_vm`, `end_vm`].  
3. The above is a serial script which will take a long time. So, you can use `./pull_diff_files_and_xfsmonkey_log_parallel.sh <dir>` which pulls the necessary files from all the VMs. Similar to `scp_segregated_workloads_parallel.ssh` script, you should change this parallel script also if the number of live nodes changes.  
4. Use `ps aux | grep pull_diff_files` to check if the pull script has completed. Once it's complete, you can find all the log and diff files under `dir` specified.  
