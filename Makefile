#-----------------------------------------------------
#
# Configuration directives are in Makefile.config
#
#-----------------------------------------------------

all: liblwgeom loaderdumper utils

install: all liblwgeom-install loaderdumper-install

uninstall: liblwgeom-uninstall loaderdumper-uninstall

clean: liblwgeom-clean loaderdumper-clean test-clean
	rm -f lwpostgis.sql

distclean: clean
	rm -Rf autom4te.cache
	rm -f config.log config.cache config.status

maintainer-clean:
	@echo '------------------------------------------------------'
	@echo 'This command is intended for maintainers to use; it'
	@echo 'deletes files that may need special tools to rebuild.'
	@echo '------------------------------------------------------'
	$(MAKE) distclean
	$(MAKE) -C lwgeom maintainer-clean
	rm -f configure

test: liblwgeom
	$(MAKE) -C regress test

test-clean:
	$(MAKE) -C regress clean

liblwgeom: 
	$(MAKE) -C lwgeom

liblwgeom-clean:
	$(MAKE) -C lwgeom clean

liblwgeom-install:
	$(MAKE) -C lwgeom install

liblwgeom-uninstall:
	$(MAKE) -C lwgeom uninstall

loaderdumper:
	$(MAKE) -C loader

loaderdumper-clean:
	$(MAKE) -C loader clean

loaderdumper-install:
	$(MAKE) -C loader install

loaderdumper-uninstall:
	$(MAKE) -C loader uninstall

utils:
	$(MAKE) -C utils

.PHONY: utils
