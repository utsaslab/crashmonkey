# Bugs reproduced with XFSMonkey

### generic_034 ###
If a directory entry is both found in the persisted metadata and in the fsync log, at log replay time the directory entry should update the right value of i_size for the files created before a crash. After recovery, removing all the files in this directory should ensure that the directory is removable. 

**Result** : Fails on btrfs (kernel 3.13). [The directory is unremovable due to incorrect i_size.](https://patchwork.kernel.org/patch/4864571/)

**Output** :
```
Reordering tests ran 64 tests with
	passed cleanly: 22
	passed fixed: 0
	fsck required: 0
	failed: 42
		old file persisted: 0
		file missing: 0
		file data corrupted: 0
		file metadata corrupted: 42
		other: 0

```

### generic_039 ###
If we decrement link count for a file, fsync and crash, on replaying the log tree, the i_size due to the removed link should be decremented. Also there should be no dangling directory index references that makes the directory unremovable. 

**Result** : Fails on btrfs (kernel 3.13). [The parent directory is unremovable due to incorrect i_size.](https://patchwork.kernel.org/patch/6058101/)

**Output** :
```
Reordering tests ran 476 tests with
	passed cleanly: 213
	passed fixed: 0
	fsck required: 0
	failed: 263
		old file persisted: 0
		file missing: 0
		file data corrupted: 0
		file metadata corrupted: 263
		other: 0

```

### generic_041 ###
Consider an inode with a large number of hard links, some of which may be extrefs. If we turn a regular ref into an extref, fsync the inode and then crash, the filesystem should be consistent and mountable. 

**Result** : Fails on btrfs (kernel 3.13). Fsync log makes the replay code always fail with -EOVERFLOW when processing the inode's references. [Makes the filesystem unmountable.](https://www.spinics.net/lists/linux-btrfs/msg41157.html)

**Output** :
```
Reordering tests ran 278 tests with
	passed cleanly: 102
	passed fixed: 0
	fsck required: 176
	failed: 0
		old file persisted: 0
		file missing: 0
		file data corrupted: 0
		file metadata corrupted: 0
		other: 0

```

### generic_059 ###
If we punch a hole for a small range (partial page), and fsync the file, the operation should persist. 

**Result** : Fails on btrfs (kernel 3.13). [Inode item was not updated, making fsync a no-op.](https://patchwork.kernel.org/patch/5830801/)

**Output** :
```
Reordering tests ran 680 tests with
	passed cleanly: 561
	passed fixed: 0
	fsck required: 0
	failed: 119
		old file persisted: 0
		file missing: 0
		file data corrupted: 119
		file metadata corrupted: 0
		other: 0

```

### generic_066 ###
Test that if we delete a xattr from a file and fsync the file, after log replay the file should not have the deleted xattr. 

**Result** : Fails on btrfs (kernel 3.13). [The deleted xattr appears in the file, even after fsync.](https://www.spinics.net/lists/linux-btrfs/msg42162.html)

**Output** :
```
Reordering tests ran 680 tests with
	passed cleanly: 561
	passed fixed: 0
	fsck required: 0
	failed: 119
		old file persisted: 0
		file missing: 0
		file data corrupted: 119
		file metadata corrupted: 0
		other: 0

```

### generic_336 ###
Suppose we move a file from one directory to another and fsync the file. Before this, the directory inode containing this file must have been fsynced/logged. If we crash and remount the filesystem, file that we moved must exist


**Result** : Fails on btrfs (kernel 4.4). [The file that we moved is lost.](https://patchwork.kernel.org/patch/8293181/)

**Output** :
```
Reordering tests ran 1000 tests with
	passed cleanly: 678
	passed fixed: 0
	fsck required: 0
	failed: 322
		old file persisted: 0
		file missing: 322
		file data corrupted: 0
		file metadata corrupted: 0
		other: 0

```

### generic_341 ###
Test that if we rename a directory, create a new directory that has the old name of our former directory and is a child of the same parent directory, fsync the new inode, power fail and mount the filesystem, we see our first directory with the new name and no files under it were lost.

**Result** : Fails on btrfs (kernel 4.4). [The moved directory and all its files are lost](https://www.spinics.net/lists/linux-btrfs/msg53591.html)

**Output** :
```
Reordering tests ran 74 tests with
	passed cleanly: 30
	passed fixed: 0
	fsck required: 0
	failed: 44
		old file persisted: 0
		file missing: 44
		file data corrupted: 0
		file metadata corrupted: 0
		other: 0

```

### generic_343 ###
If we move a directory to a new parent and later log that parent and don't explicitly log the old parent, when we replay the log we should not end up with entries for the moved directory in both the old and new parent directories.

**Result** : Fails on btrfs (kernel 4.4). [The moved directory is present in both old and new parent](https://patchwork.kernel.org/patch/8766401/)

**Output** :
```
Reordering tests ran 1000 tests with
	passed cleanly: 678
	passed fixed: 0
	fsck required: 0
	failed: 322
		old file persisted: 322
		file missing: 0
		file data corrupted: 0
		file metadata corrupted: 0
		other: 0

```

### generic_348 ###
If we create a symlink, fsync its parent directory, power fail and mount again the filesystem, the symlink should exist and its content must match what we specified when we created it (must not be empty or point to something else)

**Result** : Fails on btrfs (kernel 4.4). [We end up with an empty symlink](https://patchwork.kernel.org/patch/9158353/)

**Output** :
```
Reordering tests ran 1000 tests with
	passed cleanly: 968
	passed fixed: 0
	fsck required: 0
	failed: 32
		old file persisted: 0
		file missing: 0
		file data corrupted: 0
		file metadata corrupted: 32
		other: 0

```

### generic_376 ###
Rename a file without changing its parent directory, create a new file that has the old name of the file we renamed and fsync the file we renamed. Rename should work correctly and after a power failure both file should exist.

**Result** : Fails on btrfs (kernel 4.4). [The new file created is lost](https://patchwork.kernel.org/patch/9297215/)

**Output** :
```
Reordering tests ran 1000 tests with
	passed cleanly: 950
	passed fixed: 0
	fsck required: 0
	failed: 50
		old file persisted: 0
		file missing: 50
		file data corrupted: 0
		file metadata corrupted: 0
		other: 0

```

### generic_468 ###
Test that fallocate with KEEP_SIZE followed by a fdatasync then crash, we see the right number of allocated blocks. 

**Result** : Fails on ext4 and f2fs (kernel 4.4). [The blocks allocated beyond EOF are all lost.](https://patchwork.kernel.org/patch/10120293/)

**Output** :
f2fs:
```
Timing tests ran 3 tests with
	passed cleanly: 1
	passed fixed: 0
	fsck required: 0
	failed: 2
		old file persisted: 0
		file missing: 0
		file data corrupted: 0
		file metadata corrupted: 0
		incorrect block count: 2
		other: 0

```

ext4:
```
Timing tests ran 3 tests with
	passed cleanly: 2
	passed fixed: 0
	fsck required: 0
	failed: 1
		old file persisted: 0
		file missing: 0
		file data corrupted: 0
		file metadata corrupted: 0
		incorrect block count: 1
		other: 0

```