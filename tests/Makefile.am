# Makefile.am for Xastir tests
# Autotest-based test suite for interface helper functions

TESTSUITE = $(srcdir)/testsuite
AUTOTEST = $(AUTOM4TE) --language=autotest

TESTSUITE_AT = testsuite.at interface_helpers.at db_tests.at object_utils_tests.at output_my_aprs_data_tests.at util_tests.at
EXTRA_DIST = $(TESTSUITE_AT) $(TESTSUITE) package.m4 atlocal.in

# Test programs
check_PROGRAMS = test_interface_helpers test_db test_object_utils test_output_my_aprs_data test_util

test_interface_helpers_SOURCES = test_interface_helpers.c
test_interface_helpers_CPPFLAGS = -I$(top_srcdir)/src


test_db_SOURCES = test_db.c test_db_stubs.c $(top_srcdir)/src/db.c
test_db_CPPFLAGS = $(CPPFLAGS) -I$(top_srcdir) -I$(top_srcdir)/src -I$(top_builddir)
#test_db_LDADD = -L$(top_builddir)/src/rtree -lrtree


test_object_utils_SOURCES = test_object_utils.c test_object_utils_stubs.c $(top_srcdir)/src/object_utils.c
test_object_utils_CPPFLAGS = $(CPPFLAGS) -I$(top_srcdir) -I$(top_srcdir)/src -I$(top_builddir)

test_output_my_aprs_data_SOURCES = test_output_my_aprs_data.c mock_output_my_aprs_data.c \
      $(top_srcdir)/src/interface.c
test_output_my_aprs_data_CPPFLAGS = $(CPPFLAGS) -I$(top_srcdir) -I$(top_srcdir)/src -I$(top_builddir)
test_output_my_aprs_data_LDADD = -lpthread

test_util_SOURCES = test_util.c test_util_stubs.c $(top_srcdir)/src/util.c
test_util_CPPFLAGS = $(CPPFLAGS) -I$(top_srcdir) -I$(top_srcdir)/src -I$(top_builddir)

# Run tests
check-local: atconfig atlocal $(TESTSUITE)
	$(SHELL) '$(TESTSUITE)' $(TESTSUITEFLAGS)

# Install check
installcheck-local: atconfig atlocal $(TESTSUITE)
	$(SHELL) '$(TESTSUITE)' AUTOTEST_PATH='$(bindir)' $(TESTSUITEFLAGS)

# Clean test artifacts
clean-local:
	test ! -f '$(TESTSUITE)' || $(SHELL) '$(TESTSUITE)' --clean
	rm -rf testsuite.dir

# Convenience targets
check-unit: check-local

AUTOM4TE = autom4te

# Generate testsuite
$(TESTSUITE): $(TESTSUITE_AT) package.m4
	$(AUTOTEST) -I '$(srcdir)' -o $@.tmp $@.at
	mv $@.tmp $@

# Generate atconfig
atconfig: $(top_builddir)/config.status
	cd $(top_builddir) && $(SHELL) ./config.status tests/$@

# Generate package.m4
package.m4: $(top_srcdir)/configure.ac
	{ \
	  echo '# Signature of the current package.'; \
	  echo 'm4_define([AT_PACKAGE_NAME],      [@PACKAGE_NAME@])'; \
	  echo 'm4_define([AT_PACKAGE_TARNAME],   [@PACKAGE_TARNAME@])'; \
	  echo 'm4_define([AT_PACKAGE_VERSION],   [@PACKAGE_VERSION@])'; \
	  echo 'm4_define([AT_PACKAGE_STRING],    [@PACKAGE_STRING@])'; \
	  echo 'm4_define([AT_PACKAGE_BUGREPORT], [@PACKAGE_BUGREPORT@])'; \
	} > $(srcdir)/package.m4

.PHONY: check-unit
