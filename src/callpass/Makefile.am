#
# $Header$
#

MAINTAINERCLEANFILES=Makefile.in

bin_PROGRAMS = callpass

callpass_SOURCES = callpass.c

callpass_LINK=$(CC) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@

#install-local-hook:
#	mv $(bindir)/callpass $(prefix)/xastir/bin/callpass
#	chmod 755 $(prefix)/xastir/bin/callpass

