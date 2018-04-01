
### generic 002
xfstests - https://github.com/kdave/xfstests/blob/master/tests/generic/002  

This tests if the link count of a file is tracked properly. It creates a set of new links and removes the set of links added one by one, forcing a fsync and checkpoint between each add or remove. check_test() function tests if the file has correct number of links  according to the last checkpoint noted.

### generic 056
xfstests - https://github.com/kdave/xfstests/blob/master/tests/generic/056  

This tests for a bug in which data of a file gets lost after creating a link to the file and doing a fsync for some other file. It creates a new file foo, writes some contents, does a fsync,  creates a link to the file, creates a new file bar and fsyncs it and then crashes. After recovery, it checks if file foo contains the original contents written to it.    

### generic 106
xfstests - https://github.com/kdave/xfstests/blob/master/tests/generic/106  

This tests if the file metadata is consistent after adding and removing a link to a file followed by dropping cache and crash. It creates a new file foo, adds a link foo_link to the file foo, and syncs. It then unlinks foo_link, drops the caches, and fsyncs foo. After a crash, removing the file foo should enable removing its directory.  

### generic 107
xfstests - https://github.com/kdave/xfstests/blob/master/tests/generic/107  

This tests if the file metadata is consistent after adding and removing a link to a file followed by crash, when the links are within a different directory. It creates a new file foo, adds links foo_link and foo_link2 to the file foo inside a sub-directory, and syncs. It then unlinks foo_link2, and fsyncs foo. After a crash, removing the link foo_link should enable removing its directory.  

### generic 322
xfstests - https://github.com/kdave/xfstests/blob/master/tests/generic/322  

This tests if renaming a file followed by fsync survives a crash. It creates a new file, writes some contents, renames it to a different file, and then does a fsync. After a crash, it checks if the new file is present and has the same contents as written to the original file.  
generic 322_2 is another subtest of generic 322 where it writes new contents again and does a sync again before doing the rename.

### generic 335
xfstests - https://github.com/kdave/xfstests/blob/master/tests/generic/335  

Test that if we move one file between directories, fsync the parent directory of the old directory, power fail and remount the filesystem, the file is not lost and it's located at the destination directory.  

1. Create directories A/B and C under TEST_MNT  
2. Create file foo in dir A/B, and sync  
3. Move file foo to dir C  
4. Create another file bar in dir A  
5. fsync dir A  

If a power fail occurs now, and remount the filesystem, file bar should be present under A and file foo should be present under directory C.  

