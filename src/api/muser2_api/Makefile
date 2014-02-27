### Makefile --- 
##
## Author:   antonb
##
## Note: make sure to pass extras=1 on command line
##
## make extras=1 static=1 apilib to build the static library
##

#-------------------------------------------------------------------------------
# Define source root, the target name and include standard defs
#-------------------------------------------------------------------------------

ROOT = ../../..

-include $(ROOT)/makefile-includes    # Configuration of BOLT

TRGT = muser2_api

INCS = include idman clset mus-2 wraps-2 wraps

LIBS = clset mus-2 wraps-2 wraps

-include $(MKDIR)/makefile-common-defs

apilib: $(OBJS) libs externallibs
	@echo "Creating archive lib$(TRGT).a ..."
	@rm -f lib$(TRGT).a
	@mkdir __build
	@mv $(OBJS) ./__build
	@(cd ./__build ; \
	ar -cq lib$(TRGT).a *.o ; \
	mv $(OBJS) .. ; \
	for f in $(LIBS); do ar -x ../$(SRCDIR)/$$f/*.a ; ar -q lib$(TRGT).a *.o ; rm -f *.o ; done ; \
	for f in $(EXTERNALLIBS); do ar -x ../$(EXTDIR)/$$f/*.a ; ar -q lib$(TRGT).a *.o ; rm -f *.o ; done ; \
	mv lib$(TRGT).a .. ; \
	cd ..)
	@rm -fr ./__build
	@echo "Done."

#-------------------------------------------------------------------------------

### Makefile ends here
