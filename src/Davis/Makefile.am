
# $Id$
#
# Copyright (C) 2004  The Xastir Group
#

AUTOMAKE_OPTIONS = gnu dist-bzip2
ACLOCAL_AMFLAGS = -I m4

D=      `date +%G%m%d.%H%M%S`

SUBDIRS = src

# Remove this file here (it is created via configure), not from within intl.
# DISTCLEANFILES =

EXTRA_DIST = aclocal.m4 AUTHORS bootstrap.sh ChangeLog config.h.in \
        config.status configure.ac COPYING INSTALL NEWS \
        README bootstrap.sh

MAINTAINERCLEANFILES = configure config.status aclocal.m4 \
        Makefile.in Makefile config.guess config.sub install-sh \
        missing mkinstalldirs

docdir=${pkgdatadir}/doc
#doc_DATA=AUTHORS FAQ ChangeLog LICENSE README README.CVS \
#        README.MAPS README.win32 COPYING INSTALL UPGRADE

mandir=${prefix}/man
#man_MANS=xastir.1

ChangeLog::


