# Automatic Crash Explorer (Ace) #

### Overview ###
Ace is a bounded, and exhaustive workload generator for POSIX file systems. A workload is simply a sequence of file-system operations. Ace comprises of two main components

1. **High-level workload generator** : This is responsible for exhaustively generating workloads within the defined bounds. The generated workloads are represented in a high-level language which resembles the one below.
```
mkdir B 0777
open Bfoo O_RDWR|O_CREAT 0777
fsync Bfoo
checkpoint 1
close Bfoo
```

1. **Workload Adapter** : Workloads represented in the high-level language have to be converted into executables that can be run and verified. We support the following two formats:
    1. *Crashmonkey* : The Crashmonkey adapter translates the high-level language into a [format](workload.md) that CrashMonkey understands. For example, the above workload is converted to the run method of CrashMonkey as follows.

```c++
virtual int run( int checkpoint ) override {

    test_path = mnt_dir_ ;
    B_path =  mnt_dir_ + "/B";
    Bfoo_path =  mnt_dir_ + "/B/foo";

    int local_checkpoint = 0 ;

    if ( mkdir(B_path.c_str() , 0777) < 0){
      return errno;
    }

    int fd_Bfoo = cm_->CmOpen(Bfoo_path.c_str() , O_RDWR|O_CREAT , 0777);
    if ( fd_Bfoo < 0 ) {
      cm_->CmClose( fd_Bfoo);
      return errno;
    }

    if ( cm_->CmFsync( fd_Bfoo) < 0){
      return errno;
    }

    if ( cm_->CmCheckpoint() < 0){
      return -1;
    }
    local_checkpoint += 1;
    if (local_checkpoint == checkpoint) {
      return 1;
    }

    if ( cm_->CmClose ( fd_Bfoo) < 0){
      return errno;
    }

    return 0;
}
```

2. *XFSTest* : The XFSTest adapter translates the high-level language into a test file and expected output file to be run with [xfstest](https://github.com/kdave/xfstests). For example, the above workload would be converted into the following code (excluding the xfstest initializiation and code and helper methods):
	
```bash
mkdir $SCRATCH_MNT/B -p -m 0777
touch $SCRATCH_MNT/B/foo
chmod 0777 $SCRATCH_MNT/B/foo
$XFS_IO_PROG -c "fsync" $SCRATCH_MNT/B/foo
check_consistency $SCRATCH_MNT/B/foo
clean_dir
```

The definition of `check_consistency` and `clean_dir` as well as all other helper functions can be found [here](../code/tests/ace-base/base_xfstest.sh). The [XFSTest adapter](../ace/xfstestAdapter.py) itself can be run with the following required arguments:

```
--test_file TEST_FILE,             -t TEST_FILE       J lang test skeleton to generate workload
--target_path TARGET_PATH,         -p TARGET_PATH     Directory to save the generated test files
--test_number TEST_NUMBER,         -n TEST_NUMBER     The test number following xfstest convention. 
                                                      Will generate <test_number> and <test_number>.out 
--filesystem_type FILESYSTEM_TYPE, -f FILESYSTEM_TYPE The filesystem type for the test 
                                                      (i.e. generic, ext4, btrfs, xfs, f2fs, etc.)
```

For example, in running the following:

```
python2 xfstestAdapter.py -t <J-LANG FILE> -p output/ -n 001 -f generic
```

would create `output/001` and `output/001.out` from the given J-lang file. 

For more information explaining how to run the adapters directly, refer to the beginning of the following files for [Crashmonkey](../ace/cmAdapter.py) and [xfstest](../ace/xfstestAdapter.py) respectively.

3. *XFSTEST-Concise* : Ace can also generate J-lang version two files (or J2-lang files) which contain multiple tests wrapped into a single file. The *XFSTest* adapter automatically detects what version of J-lang file is passed in, and decides whether to generate a single test or a condensed test. The adapter can be run with the same arguments as described above, with J-lang files replaced by J2-lang files. For example, running:
 
```
python2 xfstestAdapter.py -t <J2-LANG FILE> -p output/ -n 001 -f generic
```

would create `output/001` and `output/001.out` from the given J2-lang file.

---
### Bounds used by Ace ###

Ace currently generates workloads of sequence length 1, 2, and 3. We say a workload has sequence length 'x' if it has 'x' core file-system operations in it. Currently, Ace places the following bounds.

1. **Sequence length** : Only sequences of length up to 3 have been tested so far.

2. **File-system operations** : Ace currently supports the following system calls :

    `creat, mkdir, falloc, write, direct write, mmap write, link, unlink, remove, rename, fsetxattr, removexattr, truncate, fdatasync, fsync, sync, symlink`

    However, be cautious of the number of workloads that can be generated in each sequence length, if you decide to test this whole set of operations. As you go higher up the sequence length, it is advisable to narrow down the operation set, or increase the compute available to test.

3. **File set** : Ace supports only a pre-defined file set. You have the freedom to restrict it to a smaller set. Currently, we support 4 directories and 2 files within each directory.

    ```
    dir : test (files: foo, bar)
          |
          |__ dir : A (files: foo, bar)
          |         |
          |         |_ dir C (files: foo, bar)
          |
          |__ dir : B (files: foo, bar)
    ```

    However, by default, we only use directory A, B, and the files under them. To include a level of nesting, add the files under test dir or C to the file set.

4. **Persistence operations** : Ace supports four types of persistence operations to be included in the workload

    `sync, fsync, fdatasync, none`
    Since by default `fdatasync` is one of the file-system operations under test, it is excluded from the list of persistence operations. However you could add it to the persistence operation set in the ace script.

5. **Fileset for persistence operations** : Ideally, the file set for persistence operations should be the same as the above defined file set. However, there is no point persisting a file that was not touched by any of the system calls in the workload. Hence, we reduce the space of workloads further by restricting the file set for persistence operations:
      1. Range of used files : In this mode, in addition to the files/directories involved in the workload, we allow persisting sibling files and parent directory. This is the default for seq-1 and seq-2.
      2. Strictly used files : In this mode, only files/directories acted upon by the workload can be persisted. We default to this mode in seq-3, to restrict the workload set.

___
### Generating workloads with Ace ###

Generating workloads with Ace is a two-step process.

  1. Set the bounds that you wish to explore, using the guidelines and advice in the [previous](#bounds-used-by-ace) section.

  2. Start workload generation.

      ```python
      cd ace
      python ace.py -l <seq_length> -n <True|False> -d<True|False>
      ```
      For example, to generate seq-2 workloads with no additional nested directory, run :
      ```python
      python ace.py -l 2 -n False -d false
      ```

      **Flags:**

      * `-l` - Sequence length of the workload, i.e., the number of core file-system operations in the workload.
      * `-n` - If True, provides an additional level of nesting to the file set. Adds a directory `A/C` and two files `A/C/foo` and `A/C/bar` to the set of files.
      * `-d` - Demo workload. If true, simply restricts the workload space to test two file-system operations `link` and `fallocate`, allowing the persistence of used files only. The file set is also restricted to just `foo` and `A/bar`
			* `-t` - The type of test to generate. Should be one of 'crashmonkey', 'xfstest', and 'xfstest-concise'. If unspecified, the adapter will default to 'crashmonkey'.

___
### Generalizing Ace ###
You can extend Ace to generate workloads of larger sequences, expand the set of files and directories acted upon, or support new file-system operations. Let's see what changes are required to do so.

#### Generating workloads of larger sequences ####

There is nothing preventing Ace from generating workloads of length > 3. You could simply say `-l 4` to generate sequence 4 workloads. One bit of dependency that you should add in for sequences 5 and above is, is the insertion of persistence operations.

```python
if int(num_ops) == 4:
    for i in itertools.product(SyncSetCustom, SyncSetCustom, SyncSetCustom, SyncSetNoneCustom):
        syncPermutationsCustom.append(i)
```
This piece of code ensures that the last persistence op is never `None`. We need to generalize this in [Ace](../ace/ace.py), to handle higher sequences.

#### Adding more files and directories ####

Workload generation is Ace is somewhat closely coupled to the set of files and directories available. While we aim to make it general enough to handle any user-defined file, it currently requires quite a few modifications in [Ace](../ace/ace.py) to support new files. You need to ensure that the methods `SiblingOf(file)` and `Parent(file)` return appropriate values for the new file you are adding. The functions to check dependencies are also tied to our pre-defined list of files. For example, we need to generalize `checkDirDep` and `checkParentExistsDep` to include the new file. In future, we aim to generalize these functions to understand this layout of files and directories.

#### Supporting new file-system operations ####

Supporting new file-system operations requires additions to both [Ace](../ace/ace.py) and [Adapter](../ace/cmAdapter.py). First, add the new system call to the `buildTuple` function, with appropriate argument list to this operation. Next, ensure that all the dependencies arising due to this system call are addressed in the `satisfyDep` function. Finally, add the new system call to the `buildJlang` function, in a format you would like to see it in the high-level language test file. Now for the adapter, add the new system call to the `insertFunctions` method, and write an appropriate `insert<NewSysCall>` method that handles the conversion of high-level language description to C++ equivalent code.
