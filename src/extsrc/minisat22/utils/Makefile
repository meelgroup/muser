### Makefile --- 

## Author: jpms
## Keywords: 
## X-URL: 

#-------------------------------------------------------------------------------
# Define source root, the target name and include standard defs
#-------------------------------------------------------------------------------

ROOT = ../../../..

TRGT = minisat22

lib:	rm-lib-lnks

-include $(ROOT)/makefile-includes    # Configuration of BOLT

INCS += includes idman
EXTINCS += minisat22/inc

-include $(MKDIR)/makefile-common-defs

# STATS may be slower...
#CFLAGS := $(CFLAGS) -DTRACE
#CFLAGS := $(CFLAGS) -DTRACE -DSTATS

CXXFLAGS := $(CFLAGS)

lib:	add-lib-lnks

rm-lib-lnks:
	@-(cd ..; rm -f lib$(TRGT).a)

add-lib-lnks:
	@-(cd ..; ln -s core/lib$(TRGT).a .)


#-------------------------------------------------------------------------------

### Makefile ends here
