### Makefile --- 
##
## Author: jpms, antonb
##

#-------------------------------------------------------------------------------
# Include config file. Set src root, target name, include dirs and required libs
#-------------------------------------------------------------------------------

ROOT = ../../..

-include $(ROOT)/makefile-includes    # Configuration of BOLT

XTRGT = muser2

CPPFLAGS += 

INCS = include idman clset mus-2 wraps-2 wraps # important: wraps-2 must be included before wraps to get MUSer2:: versions
ifeq ($(xm), xp)
EXTINCS = minisat-abbr # this solver needs special configuation
endif
PARSEINCS = chkfmt cnffmt gcnffmt vgcnffmt

# order matters here (keep clset last) because of static libs
LIBS = mus-2 wraps-2 wraps clset

LFLAGS = -lz

# Order in LNKFLAGS matters (because of static libraries)
LNKFLAGS := $(LNKFLAGS) $(LFLAGS)

# Additions for stats using clock_gettime(2) on Linux
ifeq ($(findstring Linux, $(shell uname)), Linux)
LNKFLAGS += -lrt
endif

#
# When mt=1 is given to the make, the tool is built in multi-threaded mode
#
ifeq ($(mt), 1)
CPPFLAGS += -DMULTI_THREADED
# Additions for TBB (NOTE: had to adjust IDIRS and LLIBDIRS b/c of different
# versions of TBB for Linux and Mac)
ifeq ($(findstring Darwin, $(shell uname)), Darwin)
   IDIRS += -I$(ROOT)/src/extutil/tbb30/mac/include
   LLIBDIRS += -L$(ROOT)/src/extutil/tbb30/mac/lib 
endif
ifeq ($(findstring Linux, $(shell uname)), Linux)
   IDIRS += -I$(ROOT)/src/extutil/tbb30/lin/include
   # choose the correct combination here between the two (the version of libc 
   # and the cc used to build it can be viewed by executing /lib/libc.so.6 from 
   # shell)
   #ARCH = intel64/cc3.4.3_libc2.3.4_kernel2.6.9  
   ARCH = intel64/cc4.1.0_libc2.4_kernel2.6.16.21   
   LLIBDIRS += -L$(ROOT)/src/extutil/tbb30/lin/lib/$(ARCH) 
endif

ifeq ($(mkm), dbg)
   CPPFLAGS += -DTBB_USE_DEBUG
   LNKFLAGS += -ltbb_debug
ifeq ($(findstring Linux, $(shell uname)), Linux)
   # add scalable memory allocator drop-in on linux
   LNKFLAGS += -ltbbmalloc_proxy_debug -ltbbmalloc_debug
endif
else
   LNKFLAGS += -ltbb
ifeq ($(findstring Linux, $(shell uname)), Linux)
   # add scalable memory allocator drop-in on linux
   LNKFLAGS += -ltbbmalloc_proxy -ltbbmalloc
endif
endif
endif

# main target 

all: exec

-include $(MKDIR)/makefile-common-defs


#-------------------------------------------------------------------------------

### Makefile ends here
