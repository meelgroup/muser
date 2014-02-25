### Makefile --- 
##
## Author: jpms, antonb
##

#-------------------------------------------------------------------------------
# Include config file. Set src root, target name, include dirs and required libs
#-------------------------------------------------------------------------------

ROOT = ../../..

-include $(ROOT)/makefile-includes    # Configuration of BOLT

XTRGT = muser-2

INCS = include idman clset mus-2 wraps-2
PARSEINCS = cnffmt gcnffmt

LIBS = clset mus-2 wraps-2 
EXTERNALLIBS = minisat22-2
PARSELIBS = 
LIBLIBS = 

INCFILES = 

LFLAGS = -lz

# Order in LNKFLAGS matters...
LNKFLAGS := $(LNKFLAGS) $(LFLAGS)

# additions for the custom-built gcc on linuxes (remove if gcc is in standard place)
ifeq ($(findstring Linux, $(shell uname)), Linux)
    ifeq ($(m32), 1)
	LNKFLAGS += -Wl,-rpath,$(HOME)/lib32
    else
	LNKFLAGS += -Wl,-rpath,$(HOME)/lib
    endif
endif

all: exec

-include $(MKDIR)/makefile-common-defs


#-------------------------------------------------------------------------------

### Makefile ends here
