# $Id$
#
# Copyright (C) 2000-2009  The Xastir Group
#

pkglib_SCRIPTS = \
    Coordinate.pm \
    coord-convert.pl \
    get-fcc-rac.pl \
    get-gnis \
    get-maptools.sh \
    get-NWSdata \
    get_shapelib.sh \
    gpx2shape \
    icontable.pl \
    inf2geo.pl \
    kiss-off.pl \
    langElmerFudd.pl \
    langMuppetsChef.pl \
    langOldeEnglish.pl \
    langPigLatin.pl \
    langPirateEnglish.pl \
    mapblast2geo.pl \
    mapfgd.pl \
    object2shp.pl \
    overlay.pl \
    ozi2geo.pl \
    permutations.pl \
    split_gnis.bash \
    split_gnis.pl \
    test_coord.pl \
    toporama50k.pl \
    toporama250k.pl\
    track-get.pl \
    update_langfile.pl \
    values \
    values.pl \
    waypoint-get.pl \
    xastir-fixcfg.sh \
    xastir-migrate.sh \
    Xastir_tigerpoly.py

EXTRA_DIST = $(pkglib_SCRIPTS)

MAINTAINERCLEANFILES = $(CONFIGURE_DEPS}

SUFFIXES  = .in
am--refresh:
	@:
$(srcdir)/Makefile.in:  $(srcdir)/Makefile.am  $(am__configure_deps)
	@for dep in $?; do \
          case '$(am__configure_deps)' in \
            *$$dep*) \
              echo ' cd $(srcdir) && $(AUTOMAKE) --gnu '; \
              cd $(srcdir) && $(AUTOMAKE) --gnu  \
                && exit 0; \
              exit 1;; \
          esac; \
        done; \
        echo ' cd $(top_srcdir) && $(AUTOMAKE) --gnu  Makefile'; \
        cd $(top_srcdir) && \
          $(AUTOMAKE) --gnu  Makefile
.PRECIOUS: Makefile
Makefile: $(srcdir)/Makefile.in $(top_builddir)/config.status
	@case '$?' in \
          *config.status*) \
            echo ' $(SHELL) ./config.status'; \
            $(SHELL) ./config.status;; \
          *) \
            echo ' cd $(top_builddir) && $(SHELL) ./config.status $@ $(am__depfiles_maybe)'; \
            cd $(top_builddir) && $(SHELL) ./config.status $@ $(am__depfiles_maybe);; \
        esac;

%:	%.in
	sed -e 's:@PREFIX@:${prefix}:' >$@  <$^
