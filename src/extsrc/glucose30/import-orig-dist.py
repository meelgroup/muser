# 
# This script replaces the glucose sources with the ones from orig-dist/glucose-3.0, 
# and modifies the new sources so that they compile and link in BOLT.
#
# Run as python import-orig-dist.py from extsrc/glucose30 directory.
#
# Note: this is a stripped-down version of minisat-gh/import-orig-dist.py -- the
# latter makes more modifications, so look there first if you need anything else.
#
# (c) 2013, Anton Belov
#

from __future__ import print_function
import os
import glob
import shutil
import fileinput

# substitutions map
smap = {
    '#include "' : '#include "../glucose30/', #includes
    '"PRI' : '" PRI'                          # format macros
}

# original source root
orig_root = 'orig-dist/glucose-3.0/'

# list of subdirectories
subdirs = [ 'core', 'simp', 'utils', 'mtl' ]

# 1. create dirs, or clean them if already there
print("Cleaning files ...")
for d in subdirs:
    if os.path.exists(d):
        for f in glob.glob(d+'/*.*'): os.remove(f)
    else:
        os.mkdir(d)
        print("Warning: you will need to create local Makefiles")

# 2. copy the new files
print("Bringing new files ...")
for d in subdirs:
    for f in glob.glob(orig_root + d + '/*.*'): 
        if not f.endswith('Main.cc') and not f.endswith('.mk'): shutil.copy(f, d)

# 3. modify files
print("Making global modifications ...")
for d in subdirs:
    for f in glob.glob(d + '/*.*'):
        print("  "+f)
        for line in fileinput.input(f, inplace=True):
            for (s1,s2) in smap.iteritems():
                line = line.replace(s1, s2)
            print(line, end='')

# 4. modify mtl/IntTypes.h
print("Modifying mtl/IntTypes.h")
for line in fileinput.input('mtl/IntTypes.h', inplace=True):
    print(line, end='')
    if line.startswith('#define Glucose_'):
        print('#define __STDC_FORMAT_MACROS')
        print('#define __STDC_LIMIT_MACROS')
        print('#pragma GCC diagnostic ignored "-Wparentheses"')

# 5. modity mtl/Map.h
print("Modifying mtl/Map.h")
hashes = []
for line in fileinput.input('mtl/Map.h', inplace=True):
    if line.startswith('static inline uint32_t hash'):
        hashes.append(line)
    else:
        print(line, end='')
for line in fileinput.input('mtl/Map.h', inplace=True):
    print(line, end='')
    if line.startswith('namespace'):
        for h in hashes: print(h, end='')

# 6. silence a warning in core/Solver.cc
print("Modifying core/Solver.cc")
for line in fileinput.input('core/Solver.cc', inplace=True):
    if line.find('c last restart ## conflicts') != -1:
        print('// ', end='')
    print(line, end='')
    if line.startswith('using namespace Glucose'):
        print('#pragma GCC diagnostic ignored "-Wsign-compare"')

