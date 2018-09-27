# XfsMonkey #

### XfsMonkey ###
XfsMonkey is simply a script that runs CrashMonkey in a loop to test multiple workloads.

#### Running XFSMonkey ####
To run XFSMonkey, first get CrashMonkey up and working. XFSMonkey accepts the same parameters as CrashMonkey standalone tests. To get a list of all supported flags and their default setting, run `python xfsMonkey.py -h` in the root directory of CrashMonkey repository. If you have a directory of workloads to be tested, say `build/tests/test_workloads`, you can invoke the xfsMonkey script using `python xfsMonkey.py -t btrfs -u /build/tests/test_workloads`. This will test all the workloads under the input directory with CrashMonkey (uses auto-checker by default), and outputs the test summary in a log file `<date_timestamp>-xfsMonkey.log`. In addition, each test case that was run has a detailed log file `<date_timestamp>-test_name.log`, that can be found in the `build` directory.
