# 
# This script replaces the minisat sources with the ones from orig-dist/minisat, 
# and modifies the new sources so that they compile and link in BOLT.
#
# Run as python import-orig-dist.py from extsrc/minisat-gh directory.
#
# Assumes that subdirectory orig-dist contains the clone of minisat repo from github.
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
    'namespace Minisat' : 'namespace MinisatGH', # namespace
    'Minisat::' : 'MinisatGH::',                 # namespace
    '#ifndef Minisat_' : '#ifndef MinisatGH_',   # includes
    '#define Minisat_' : '#define MinisatGH_',   # includes
    '#include "minisat/' : '#include "../minisat-gh/', #includes
    '"PRI' : '" PRI'                             # format macros
}

# original source root
orig_root = 'orig-dist/minisat/'

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
        if not f.endswith('Main.cc'): shutil.copy(f, d)

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
    if line.startswith('#define MinisatGH_'):
        print('#define __STDC_FORMAT_MACROS')
        print('#define __STDC_LIMIT_MACROS')

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

# 6. modify core/SolverTypes.h
print("Modifying core/SolverTypes.h")
done = False
for line in fileinput.input('core/SolverTypes.h', inplace=True):
    if not done and line.startswith('#include '):
        print('#define MINISAT_CONSTANTS_AS_MACROS')
        done = True
    print(line,end='')

