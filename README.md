This is a git import from the original SVN repo for md5deep, now known as hashdeep.

It includes tags for all the releases that where in https://sourceforge.net/p/md5deep/code/HEAD/tree/ so mostly useful for historical purposes.

See upstream version at:

  * https://github.com/jessek/hashdeep
  * https://sourceforge.net/projects/md5deep/

This branch is a modified version of one release 0.15, one of the initial releases. It includes a change from the 0.16 release (the 0.16 release branch does not build and is missing implementations for a couple of functions) and a hack to provide a program name, that is just enough to build on IBM's AIX with IBM xlc 16.1.0.


## Build

### AIX Build

Works with IBM xlC 16.1.0 on PowerPC

    cc -lm -o md5deep -D__UNIX  files.c hashTable.c  match.c md5.c md5deep.c progname_hack.c
    #cc -lm -o md5deep -DMD5DEEP_GETOPT_END=255 -D__UNIX -D__PUREC__=1  files.c hashTable.c  match.c md5.c md5deep.c progname_hack.c  # also works

### Anything else Build

Works for msys gcc under Windows on Intel x64_86

    cc -lm -o md5deep  files.c hashTable.c  match.c md5.c md5deep.c


## Python sanity check

Single file md5sum implementation that can run under CPython 2.x and 3.x along with Jython 2.x:

    # should work with most versions of (C)Python

    import sys
    try:
        import hashlib
        #from hashlib import md5
        md5 = hashlib.md5
    except ImportError:
        # pre 2.6/2.5
        from md5 import new as md5

    filename = sys.argv[1]
    m = md5()
    f = open(filename, 'rb')
    data = f.read()
    f.close()
    m.update(data)
    print('%s %s' % (m.hexdigest(), filename))
