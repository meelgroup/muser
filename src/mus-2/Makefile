### Makefile --- 
##
## Author:   antonb
##
## Note: make sure to pass extras=1 on command line
##

#-------------------------------------------------------------------------------
# Define source root, the target name and include standard defs
#-------------------------------------------------------------------------------

ROOT = ../..

-include $(ROOT)/makefile-includes    # Configuration of BOLT

TRGT = mus-2
CPPFLAGS += 
INCS += includes idman clset wraps-2 wraps   # important: wraps-2 must be included before wraps to get MUSer2:: versions
EXTINCS += minisat22 minisat22/inc           # for BCESimplifier, BcpSimplifier, OrderSchedulers, VESimplifier

EXTRALIBS += 

#
# When mt=1 is given to the make, the tool is built in multi-threaded mode
#
ifeq ($(mt), 1)
CPPFLAGS += -DMULTI_THREADED
ifeq ($(findstring Darwin, $(shell uname)), Darwin)
   IDIRS += -I$(ROOT)/src/extutil/tbb30/mac/include
endif
ifeq ($(findstring Linux, $(shell uname)), Linux)
   IDIRS += -I$(ROOT)/src/extutil/tbb30/lin/include
endif
ifeq ($(mkm), dbg)
   CPPFLAGS += -DTBB_USE_DEBUG
endif
endif

-include $(MKDIR)/makefile-common-defs

#-------------------------------------------------------------------------------

### Makefile ends here
