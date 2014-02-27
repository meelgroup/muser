### Makefile --- 
##
## Author:   antonb
##
##

#-------------------------------------------------------------------------------
# Define source root, the target name and include standard defs
#-------------------------------------------------------------------------------

ROOT = ../..

-include $(ROOT)/makefile-includes    # Configuration of BOLT

TRGT = muser2_api

INCS = include idman clset mus-2 wraps-2 wraps

LIBS = clset mus-2 wraps-2 wraps

-include $(MKDIR)/makefile-common-defs

apilib: $(OBJS) libs
	@-echo "Building lib$(TRGT).a"
	@-mkdir bolt_arch
	@-cp *.o bolt_arch
	@-(cd bolt_arch; ar cqs lib$(TRGT).a *.o; rm *.o)
	@-(cd bolt_arch; for f in $(LIBS); do mkdir $$f; cd $$f; ar x ../../$(SRCDIR)/$$f/*.a ; cd ..; done; ar qs lib$(TRGT).a `find . -name "*.o"`; cd ..; mv bolt_arch/lib$(TRGT).a .)
	@-rm -fr bolt_arch
	@echo "Done."

#-------------------------------------------------------------------------------

### Makefile ends here
