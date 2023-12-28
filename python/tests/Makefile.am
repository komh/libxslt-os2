if WITH_PYTHON
TESTSPY =	\
    basic.py	\
    exslt.py	\
    extelem.py	\
    extfunc.py

exampledir = $(docdir)/python/examples
dist_example_DATA = test.xml test.xsl pyxsltproc.py $(TESTSPY)

check-local: $(TESTSPY)
	@(export PYTHONPATH="..$(PATH_SEPARATOR)../.libs$(PATH_SEPARATOR)$(srcdir)/..$(PATH_SEPARATOR)$$PYTHONPATH"; \
	  export LD_LIBRARY_PATH="$(top_builddir)/libxslt/.libs$(PATH_SEPARATOR)$(top_builddir)/libexslt/.libs$(PATH_SEPARATOR)$$LD_LIBRARY_PATH"; \
	  export DYLD_LIBRARY_PATH="$(top_builddir)/libxslt/.libs$(PATH_SEPARATOR)$(top_builddir)/libexslt/.libs$(PATH_SEPARATOR)$$DYLD_LIBRARY_PATH"; \
	  export PATH="$(top_builddir)/libxslt/.libs$(PATH_SEPARATOR)$(top_builddir)/libexslt/.libs$(PATH_SEPARATOR)$$PATH" ; \
	  if test "x$(LIBXML_SRC)" != "x" ; then \
            PYTHONPATH="$(LIBXML_SRC)/python$(PATH_SEPARATOR)$(LIBXML_SRC)/python/.libs$(PATH_SEPARATOR)$$PYTHONPATH"; \
	    LD_LIBRARY_PATH="$(LIBXML_SRC)/.libs$(PATH_SEPARATOR)$$LD_LIBRARY_PATH"; \
	    DYLD_LIBRARY_PATH="$(LIBXML_SRC)/.libs$(PATH_SEPARATOR)$$DYLD_LIBRARY_PATH"; \
	    PATH="$(LIBXML_SRC)/.libs$(PATH_SEPARATOR)$$PATH" ; \
          fi; \
	  for test in $(TESTSPY) ; do \
	    echo "-- $$test" ; \
	    $(PYTHON) $(srcdir)/$$test; \
	  done)
endif

CLEANFILES = *.pyc core
