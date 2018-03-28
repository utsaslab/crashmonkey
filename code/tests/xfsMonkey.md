
### generic 002
xfstests - https://github.com/kdave/xfstests/blob/master/tests/generic/002  

This tests if the link count of a file is tracked properly. It creates a set of new links and removes the set of links added one by one, forcing a fsync and checkpoint between each add or remove. check_test() function tests if the file has correct number of links  according to the last checkpoint noted.

### generic 322
xfstests - https://github.com/kdave/xfstests/blob/master/tests/generic/322  

This tests if renaming a file followed by fsync survives a crash. It creates a new file, writes some contents, renames it to a different file, and then does a fsync. After a crash, it checks if the new file is present and has the same contents as written to the original file.  
generic 322_2 is another subtest of generic 322 where it writes new contents again and does a sync again before doing the rename.
