### Makefile --- 

## Author:   jpms
## Keywords: 
## Copyright (c) 2007, Joao Marques-Silva

#-------------------------------------------------------------------------------
# Define source root, the target name and include standard defs
#-------------------------------------------------------------------------------

ROOT = ../..

-include $(ROOT)/makefile-includes    # Configuration of BOLT

TRGT = wraps

CPPFLAGS += 

INCS += includes tlog idman clset ucore

EXTINCS += minisat22/inc minisat-hmuc picosat954 minisat-abbr minisat-gh glucose30 lingeling-ala ubcsat12 

EXTERNALLIBS = minisat22 minisat-hmuc picosat954 minisat-abbr minisat-gh glucose30 lingeling-ala ubcsat12 

-include $(MKDIR)/makefile-common-defs

lib: externallibs
	@-echo "Building lib$(TRGT).a"
	@-mkdir bolt_arch
	@-cp *.o bolt_arch
	@-(cd bolt_arch; ar cqs lib$(TRGT).a *.o; rm *.o)
	@-(cd bolt_arch; for f in $(EXTERNALLIBS); do mkdir $$f; cd $$f; ar x ../../$(EXTDIR)/$$f/*.a ; for x in `ls *.o`; do mv $$x $$f:$$x; done; cd ..; done; ar qs lib$(TRGT).a `find . -name "*.o"`; cd ..; mv bolt_arch/lib$(TRGT).a .)
	@-rm -fr bolt_arch
	@-echo "Done"
	
# make sure all the solver's libraries are cleaned-up even when called from somewhere else
dirallclean: clean libclean
	@-(for f in $(EXTERNALLIBS); do (cd $(EXTDIR)/$$f ; make allclean) ; done)

#-------------------------------------------------------------------------------

### Makefile ends here
