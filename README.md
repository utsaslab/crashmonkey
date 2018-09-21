# Bounded Black-Box Crash Testing (B<sup>3</sup>) #
Bounded Black-Box testing (B<sup>3</sup>) is a new approach to systematically testing file-system crash consistency. B<sup>3</sup> is a black-box testing approach: no file-system code is modified. B<sup>3</sup> exhaustively generates workloads of file-system operations in a bounded space, simulates crashes after persistence operations like fsync(), and automatically tests for consistency (data and metadata of files persisted before the crash).

We built two tools, CrashMonkey and Automatic Crash Explorer (Ace) to implement the (B<sup>3</sup>) approach. Our tools can be used out of the box on any Linux filesystem that implements POSIX API. Our tools have been tested to work with ext2, ext3, ext4, xfs, F2FS, and btrfs, across the following Linux kernel versions - 3.12, 3.13, 3.16, 4.1, 4.4, 4.15, and 4.16.


### CrashMonkey ###
CrashMonkey is a file-system agnostic record-replay-and-test framework. Unlike existing tools like dm-log-writes which require a manual checker script, CrashMonkey automatically tests for data and metadata consistency of persisted files. CrashMonkey needs only one input to run - the workload to be tested. We have described the rules for writing a workload for CrashMonkey [here](documentation/workload.md). More details about the internals of CrashMonkey can be found [here](documentation/CrashMonkey.md).

### Automatic Crash Explorer (Ace) ###
Ace is an automatic workload generator, that exhaustively generates sequences of file-system operations (workloads), given certain bounds. Ace consists of a generic workload synthesizer that generates workloads in a high-level language which we call J-lang. A CrashMonkey adapter that we built converts these workloads into C++ test files that CrashMonkey can work with. More details on the current bounds imposed by Ace and guidelines on workload generation can be found [here](documentation/Ace.md).

### Research that uses our tools ###
1. *Barrier-Enabled IO Stack for Flash Storage*. Youjip Won, Hanyang University; Jaemin Jung, Texas A&M University; Gyeongyeol Choi, Joontaek Oh, and Seongbae Son, Hanyang University; Jooyoung Hwang and Sangyeun Cho, Samsung Electronics. Proceedings of the 16th USENIX Conference on File and Storage Technologies (FAST 18). [Link](https://www.usenix.org/conference/fast18/presentation/won)

### Push-button testing for seq-1 workloads ###

This repository contains a pre-generated suite of seq-1 workloads (workloads with 1 file-system operation) [here](code/tests/seq1/). Once you have set up CrashMonkey on your machine (or VM), You can simply run :

`python xfsMonkey.py -f /dev/sda -d /dev/cow_ram0 -t btrfs -e 102400 -u build/tests/seq1/ > outfile`

This will run all the 328 tests of seq-1 on `btrfs` filesystem of size `100MB`. The bug reports can be found in the folder `diff_results`. The workloads are named j-lang<1-328>, and if any of these resulted in a bug, you will see a bug report with the same name as that of the workload, describing the difference between the expected and actual state.

### Contact Info ###
Please contact us at vijay@cs.utexas.edu with any questions. Drop us a note if you are using or plan to use CrashMonkey or Ace to test your file system.
