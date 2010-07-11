
# $Id$
# Copyright (C) 2004-2010  The Xastir Group

noinst_LIBRARIES=libshape.a

bin_PROGRAMS=shpadd \
        shpcreate \
        shpdump \
        shprewind \
        shptest \
        shptreedump \
        dbfadd \
        dbfcreate \
        dbfdump
#shputils

libshape_a_SOURCES=shpopen.c \
		shptree.c \
		dbfopen.c \
		shapefil.h

shpadd_LDADD=libshape.a
shpcreate_LDADD=libshape.a
shpdump_LDADD=libshape.a
shprewind_LDADD=libshape.a
shptest_LDADD=libshape.a
shptreedump_LDADD=libshape.a
shputils_LDADD=libshape.a
dbfadd_LDADD=libshape.a
dbfcreate_LDADD=libshape.a
dbfdump_LDADD=libshape.a

EXTRA_DIST=ChangeLog LICENSE.LGPL README README.tree dbf_api.html \
	dbfadd.c dbfcreate.c dbfdump.c libtool makefile.vc makeshape.sh \
        mkinstalldirs shapelib.def shapelib.html shp_api.html \
	shpadd.c shpcreate.c shpdump.c shprewind.c shptest.c shptreedump.c \
	shputils.c stream1.out stream1.sh stream2.out stream2.sh stream3.out
DIST_SUBDIRS=contrib
SUBDIRS=contrib
