# How to write your own workload for CrashMonkey and test it? #

You don't have to worry about workload generation if you plan to use CrashMonkey alongside Ace. However, if you have a specific workload in mind that you want to test with CrashMonkey, this section will walk you through the process of writing such a workload.

### Interface ###
The interface for CrashMonkey test case is as follows:

```c++
class BaseTestCase {
  public:
    virtual ~BaseTestCase() {};

    /*
    The initial file system state you want to start with,
    is set in this method. The block IOs resulting from the
    system calls in this method are not recorded.
    */
    virtual int setup() = 0;

    /*
    This is the core of the test file. Your actual workload
    goes in here. The kernel modules record block IOs
    resulting from this run, and capture appropriate
    snapshots to enable automatic checking.
    */
    virtual int run(const int checkpoint) = 0;

    /*
    If you additionally want to include user-defined
    consistency checks, this is the place to do so. This
    method is invoked after file-system recovery at each
    crash point.
    */
    virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) = 0;

};
```

___
### Converting a workload into CrashMonkey test case ###
Let us write a CrashMonkey test case for the following simple workload:

```
setup:
      mkdir A
      create A/foo
      sync

run:
      mkdir B
      link A/foo B/foo
      fsync A/foo
      **Mark this point**

check:
      Do A/foo and B/foo exist if you crash after the **Mark**?
```


#### Setup ####

```c++
virtual int setup() override {

  // initialize paths.
  // mnt_dir_ points to the root directory by default
    A_path = mnt_dir_ + "/A";
    Afoo_path =  mnt_dir_ + "/A/foo";

    // create the directory
    if ( mkdir(A_path.c_str() , 0777) < 0 ){
        return errno;
    }

    // create the file
    int fd_Afoo = open(Afoo_path.c_str() , O_RDWR|O_CREAT , 0777);
    if ( fd_Afoo < 0 ) {
      close( fd_Afoo);
      return errno;
    }

    sync();

    //close the file decriptor.
    if ( close ( fd_Afoo ) < 0 ) {
        return errno;
    }

    return 0;
}
```

So, as you notice, it's just normal C++ code. There are no wrapper function calls in the setup phase, because we do not record IO resulting from this phase. The interesting part is yet to come!

#### Run ####

Unlike the setup phase, you need wrappers around system calls in the run method. This is because, CrashMonkey profiles the workload to be able to track the list of files and directories being persisted. This information is necessary to enable automatic testing. Wrapping the system call is simple. For example, `open` is replaced by `cm_->CmOpen`. Similarly, `rename` is replaced by `cm_->CmRename`.  Currently, CrashMonkey requires that you wrap `open, close, rename, fsync, fdatasync, sync, msync` system calls for the correctness of auto-checker.

Another important aspect of run method is **Checkpoint**. CrashMonkey defines an optional Checkpoint flag to be inserted in the block IO sequence to indicate the completion of some operation. For example, in this workload, we should be able to check if `B/foo` is correctly recovered if a crash happened after the `fsync(A/foo)`. The auto-checker compulsorily requires that any persistence operation be followed by a checkpoint, because crashes are simulated only at these points. Let's see how to insert checkpoint in the run method.

```c++
virtual int run( int checkpoint ) override {

  int local_checkpoint = 0 ;

  < some workload >

  int fd_Afoo = cm_->CmOpen(Afoo_path.c_str() , O_RDWR|O_CREAT , 0777);
  if ( fd_Afoo < 0 ) {
    cm_->CmClose( fd_Afoo);
    return errno;
  }

  < PERSISTENCE OP 1 >

  if ( cm_->CmCheckpoint() < 0){
      return -1;
  }
  local_checkpoint += 1;
  if (local_checkpoint == checkpoint) {
      return 0;

  < continue workload >

  < LAST PERSISTENCE OP >

  if ( cm_->CmCheckpoint() < 0){
      return -1;
  }
  local_checkpoint += 1;
  if (local_checkpoint == checkpoint) {
      return 1;

  return 0;
}
```

CrashMonkey snapshots disk images at each checkpoint to create oracle for testing. Therefore, it is necessary to indicate the last checkpoint in the workload. To do so, all the checkpoints in the workload except the last one, returns 0. The last checkpoint must return 1.

#### Check_Test ####

For the above workload, we'll perform two tests.
1. Check if A/foo is present, no matter where the crash occurs, because we persisted it in the setup phase.
2. Check if the link B/foo is present, if we crashed after persisting A/foo.

Again, there is no special instruction on writing these checks. Write it as you'd do for any C++ file. However the interesting aspect here is how to communicate the bug type and a status string to the test harness. You can do this by setting the error status to one of the pre-defined types : `kFileMissing`, `kFileDataCorrupted`, etc. The complete list is available [here](../results/DataTestResult.cpp). Additionally, you can also provide a short description about the failure.

```c++
virtual int check_test( unsigned int last_checkpoint, DataTestResult *test_result) override {

  struct stat stats;
  int res_Bfoo = stat(Bfoo_path.c_str(), &stats);
  if ( last_checkpoint == 1 && res_Bfoo < 0 ) {
      test_result->SetError(DataTestResult::kFileMissing);
      test_result->error_description = " : Missing file " + Bfoo_path;
      return 0;
  }
  return 0;
}
```

___

Now that completes our test case! [See the complete test case](../code/tests/example.cpp).

You can run this workload either with auto-checker (use `-c` flag), or by skipping it and invoking only the consistency checks we defined. In either case, you should see similar results.
___
### Testing the workload you wrote ###

There are two types of testing you could perform.
1. **Test with user-defined consistency checks** : In this setting, CrashMonkey will profile the workload, generate crash states, and on the recovered file system image, it runs the **check_test** method you defined. There are two ways in which you could choose to generate crash states in this setting.

  * **Permuted replay** : By default, 10000 crash states are tested here. To generate a crash state, the recorded bios are permuted, respecting the barrier operations. However, this method is not efficient in understanding checkpoints, as these checkpoints bios could be re-ordered across checkpoints if they appear within two barrier operations. You can test the workload we just wrote, in this setting by running the following command in the `build` directory, after you compiled the test. The results of this mode of test appear under *Reordering Test* summary at test completion.

    `./c_harness -v -f /dev/sda -d /dev/cow_ram0 -t btrfs -e 102400 tests/example.so`

   * **In-order replay** : This mode does not permute the bios, it instead replays them in order until a checkpoint is reached. If a workload has *n* checkpoints, then *n* crash states are generated in this mode. The results of this mode of test are represented as *Timing Test* summary at the end of the test. You can run this mode, by simply adding a `-P` flag.

       `./c_harness -v -P -f /dev/sda -d /dev/cow_ram0 -t btrfs -e 102400 tests/example.so`

2. **Test with auto-checker** : In this setting, CrashMonkey requires the -P flag to perform in-order replay. However, to enable automatic testing, snapshots are captured at each crash point, to create an oracle representing expected state after crash. This mode does not require you to write up the check_test function. As you wrote this example test case, you would have realized that it's tedious to handcraft the checker. Especially if you have multiple checkpoints, you have to appropriately check the data and metadata of files modified between each checkpoint. Auto-checker relieves you of this manual effort. To run in this mode, use `-c flag` in addition to `-P`.

       `./c_harness -v -c -P -f /dev/sda -d /dev/cow_ram0 -t btrfs -e 102400 tests/example.so`


The log containing the recorded block IOs, the crash states tested and result at each crash state will be available at `build/<timestamp>-example.log`


#### Result ####

The result of running the above example workload on `btrfs` filesystem on any kernel <= 4.16 is as follows.

**Permuted Replay** : This mode, found no bugs with the user-defined check_test. This is because, the crash states could not understand that checkpoint 1 has been reached while checking if files A/foo and B/foo are present.
```
Reordering tests ran 10000 tests with
    passed cleanly: 10000
    passed fixed: 0
    fsck required: 0
    failed: 0
        old file persisted: 0
        file missing: 0
        file data corrupted: 0
        file metadata corrupted: 0
        incorrect block count: 0
        other: 0
```

**In-order replay** : This mode runs 1 test, because our workload has only one checkpoint. And tells you that there is some violation to the check_test you wrote. To know which check failed, open the log file.
```
Timing tests ran 1 tests with
    passed cleanly: 0
    passed fixed: 0
    fsck required: 0
    failed: 1
        old file persisted: 0
        file missing: 1
        file data corrupted: 0
        file metadata corrupted: 0
        incorrect block count: 0
        other: 0
```

In the log file, you will see the following :
```
Writing data out to each Checkpoint and checking with fsck
Test #1: FAILED: file_missing : Missing file /mnt/snapshot/B/foo
    crash state (6 bios/sectors): (1), (2), (3), (4), (5), (6)
    last checkpoint: 1
    fsck result: fsck_not_run
    fsck output:
```
This means, the second check in the check_test failed, and the link file B/foo is not persisted in the recovered file system image.

**Auto-check** : This mode also runs one test, but does not require user-defined checks. The summary block would be as follows.
```
automated check_test:
        failed: 1
```
To see the bug report, open `build/diff-at-check1`. You will see something on these lines.

```
DIFF: Content Mismatch /A/foo

/mnt/snapshot/A/foo:
---File Stat Atrributes---
Inode     : 258
TotalSize : 0
BlockSize : 4096
#Blocks   : 0
#HardLinks: 1

/mnt/cow_ram_snapshot2_0/A/foo:
---File Stat Atrributes---
Inode     : 258
TotalSize : 0
BlockSize : 4096
#Blocks   : 0
#HardLinks: 2
```

What this tells you is that, the link count of A/foo is 1, while it should have been 2. This is in turn because the link file B/foo is missing.

___
As you would have noticed by now, the workload we just wrote and tested is the [link count bug](../newBugs.md/#bug-7-:-fsync-of-file-does-not-persist-all-its-names).
