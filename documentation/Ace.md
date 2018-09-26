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

2. **CrashMonkey adapter** : Workloads represented in the high-level language have to be converted to a [format](workload.md) that CrashMonkey understands. This is taken care of by the adapter. For example, the above workload is converted to the run method of CrashMonkey as follows.

```c++
virtual int run( int checkpoint ) override {

    test_path = mnt_dir_ ;
    A_path =  mnt_dir_ + "/A";
    AC_path =  mnt_dir_ + "/A/C";
    B_path =  mnt_dir_ + "/B";
    Afoo_path =  mnt_dir_ + "/A/foo";
    Abar_path =  mnt_dir_ + "/A/bar";
    Bfoo_path =  mnt_dir_ + "/B/foo";
    Bbar_path =  mnt_dir_ + "/B/bar";
    ACfoo_path =  mnt_dir_ + "/A/C/foo";
    ACbar_path =  mnt_dir_ + "/A/C/bar";

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

---
### Bounds used by Ace ###

Ace currently generates workloads of sequence length 1, 2, and 3. We say a workload has sequence length 'x' if it has 'x' core file-system operations in it.
