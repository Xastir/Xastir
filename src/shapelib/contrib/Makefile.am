
# $Id$
# Copyright (C) 2004-2010  The Xastir Group

EXTRA_DIST=Makefile_orig ShapeFileII.pas dbfcat.c dbfinfo.c makefile.vc \
	my_nan.h shpcat.c shpcentrd.c shpdata.c shpdxf.c shpfix.c \
	shpgeo.c shpgeo.h shpinfo.c shpproj.c shpwkb.c

noinst_LIBRARIES=libshape.a

bin_PROGRAMS=dbfcat \
        dbfinfo \
        shpcat \
        shpdxf \
        shpfix \
        shpinfo
# shpcentrd
# shpdata
# shpgeo
# shpinfo
# shpproj
# shpwkb

libshape_a_SOURCES=../shpopen.c \
        ../shptree.c \
        ../dbfopen.c \
        ../shapefil.h

dbfcat_LDADD=libshape.a
dbfinfo_LDADD=libshape.a
shpcat_LDADD=libshape.a
shpdxf_LDADD=libshape.a
shpfix_LDADD=libshape.a
shpinfo_LDADD=libshape.a

