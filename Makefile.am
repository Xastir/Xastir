AUTOMAKE_OPTIONS = no-dependencies dist-shar dist-zip
ACLOCAL_AMFLAGS = -I m4

D=      `date +%G%m%d.%H%M%S`

SUBDIRS = src m4 config help symbols callpass 

# Remove this file here (it is created via configure), not from within intl.
# DISTCLEANFILES = intl/libintl.h

EXTRA_DIST = xastir.spec.in xastir.spec config.guess config.sub install-sh ltconfig \
        ltmain.sh missing mkinstalldirs TODO placeholder FAQ LICENSE README.1ST README


MAINTAINERCLEANFILES = configure aclocal.m4 Makefile.in Makefile \
        config.guess config.sub install-sh ltconfig ltmain.sh missing mkinstalldirs \
        xastir.spec

countydir=${prefix}/xastir/Counties
county_DATA=placeholder

fccdir=${prefix}/xastir/fcc
fcc_DATA=placeholder

mapdir=${prefix}/xastir/maps
map_DATA=placeholder

filleddir=${prefix}/xastir/maps/filled
filled_DATA=placeholder

transdir=${prefix}/xastir/maps/transparent
trans_DATA=placeholder

zdir=${prefix}/xastir/maps/z_overlay
z_DATA=placeholder

sounddir=${prefix}/xastir/sounds
sound_DATA=placeholder

docdir=${prefix}/xastir/doc
doc_DATA=AUTHORS FAQ ChangeLog LICENSE README.1ST COPYING

ChangeLog::


