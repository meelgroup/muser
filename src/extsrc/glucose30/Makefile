### Makefile --- 

## Author:   antonb
## Keywords: 

lib:
	@-(cd core; make obj)
	@-(cd utils; make obj)
	@-(cd simp; make obj)
	@-mkdir bolt_arch
	@-cp core/*.o bolt_arch
	@-cp utils/*.o bolt_arch
	@-cp simp/*.o bolt_arch
	@-(cd bolt_arch; ar cr libglucose30.a *.o; mv libglucose30.a ../.; rm *.o)
	@-rmdir bolt_arch

dirallclean: clean

clean:
	@-rm -f libglucose30.a
	@-(cd core; make dirallclean)
	@-(cd utils; make dirallclean)
	@-(cd simp; make dirallclean)

allclean:
	@-rm -f libglucose30.a
	@-(cd core; make dirallclean)
	@-(cd utils; make dirallclean)
	@-(cd simp; make dirallclean)

### Makefile ends here
