# How to write your own workload for CrashMonkey? #

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

```c++
setup:
      mkdir A
      fsync A

run:
      create A/foo
      write A/foo <some content>
      fsync A/foo
      **Mark this point**

check:
      Does A/foo exist and have <some content> if you crash after the **Mark**?
```
