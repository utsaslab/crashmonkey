######################
## Function Options ##
######################

FallocOptions = ['FALLOC_FL_ZERO_RANGE', 'FALLOC_FL_ZERO_RANGE|FALLOC_FL_KEEP_SIZE','FALLOC_FL_PUNCH_HOLE|FALLOC_FL_KEEP_SIZE','FALLOC_FL_KEEP_SIZE', 0]

FsyncOptions = ['fsync','fdatasync', 'sync']

# This should take care of file name/ dir name
# Default option : test, test/A [foo, bar] , test/B [foo, bar]
# We have seperated it out into two sets, first and second, in order to eliminate duplicate workloads that differ just in terms of file names.
FileOptions = ['foo', 'A/foo'] # foo
SecondFileOptions = ['bar', 'A/bar'] # bar

# A,B are  subdirectories under test
# test directory(root) is under a separate list because we don't want to try to create/remove it in the workload. But we should be able to fsync it.
DirOptions = ['A']
TestDirOptions = ['test']
SecondDirOptions = ['B']


# this will take care of offset + length combo
# Start = 4-16K , append = 16K-20K, overlap = 8000 - 12096, prepend = 0-4K

# Append should append to file size, and overwrites should be possible
# WriteOptions = ['append', 'overlap_unaligned_start', 'overlap_extend', 'overlap_unaligned_end']
WriteOptions = ['append', 'overlap_unaligned_start', 'overlap_extend'] # 'overlap_unaligned_end'


# d_overlap = 8K-12K (has to be aligned)
# dWriteOptions = ['append', 'overlap_start', 'overlap_end']
dWriteOptions = ['append', 'overlap_start'] # 'overlap_end'

# Truncate file options 'aligned'
TruncateOptions = ['aligned', 'unaligned']

# Set of file-system operations to be used in test generation.
# We currently support : creat, mkdir, falloc, write, dwrite, link, unlink, remove, rename, fsetxattr, removexattr, truncate, mmapwrite, symlink, fsync, fdatasync, sync
OperationSet = ['creat', 'mkdir', 'falloc', 'write', 'dwrite','mmapwrite', 'link', 'unlink', 'remove', 'rename', 'fsetxattr', 'removexattr', 'truncate']

# Because the filenaming convention in the high-level j-lang language 
# seems to be arbitrary, we hard code the translation here to ensure
# that any inconsistency will raise a clear, easy-to-debug error.
# The format is:
# <jlang filename> | <bash filename> | <parent jlang filename> | <file type>
JLANG_FILES = [
    ( ""      , ""        , ""  , "dir"  ),
    ( "test"  , "test"    , ""  , "file" ), 
    ( "A"     , "A"       , ""  , "dir"  ), 
    ( "AC"    , "A/C"     , "A" , "dir"  ), 
    ( "B"     , "B"       , ""  , "dir"  ), 
    ( "foo"   , "foo"     , ""  , "file" ), 
    ( "bar"   , "bar"     , ""  , "file" ), 
    ( "Afoo"  , "A/foo"   , "A" , "file" ), 
    ( "Abar"  , "A/bar"   , "A" , "file" ), 
    ( "Bfoo"  , "B/foo"   , "B" , "file" ), 
    ( "Bbar"  , "B/bar"   , "B" , "file" ), 
    ( "ACfoo" , "A/C/foo" , "AC", "file" ), 
    ( "ACbar" , "A/C/bar" , "AC", "file" )
]
