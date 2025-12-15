#
# Copyright (C) 2000-2023 The Xastir Group
#

bin_PROGRAMS = xastir xastir_udp_client testdbfawk

SUBDIRS = rtree
DIST_SUBDIRS = rtree

EXTRA_DIST=icon.xbm

XASTIR_SRC = \
    alert.c alert.h \
    ambiguity_utils.c ambiguity_utils.h \
    awk.c awk.h \
    bulletin_gui.c bulletin_gui.h \
    cad_objects.c cad_objects.h \
    color.c color.h \
    datum.c datum.h \
    debug_utils.c debug_utils.h \
    db.c db_funcs.h database.h \
    db_gis.c db_gis.h \
    db_gui.c db_gui.h \
    dbfawk.c dbfawk.h \
    dlm.c dlm.h \
    dr_utils.c dr_utils.h \
    draw_symbols.c draw_symbols.h \
    fcc_data.c fcc_data.h \
    festival.c festival.h \
    fetch_remote.c fetch_remote.h \
    forked_getaddrinfo.c forked_getaddrinfo.h \
    geocoder.c geocoder.h \
    geocoder_gui.c \
    gps.c gps.h \
    hashtable.c hashtable_private.h hashtable.h \
    hashtable_itr.c hashtable_itr.h \
    igate.c igate.h \
    interface.c interface.h \
    interface_gui.c \
    lang.c lang.h \
    leak_detection.h \
    list_gui.c list_gui.h \
    locate_gui.c \
    location.c \
    location_gui.c \
    log_utils.c log_utils.h \
    main.c main.h \
    maps.c maps.h \
    map_cache.c map_cache.h \
    map_dos.c \
    map_geo.c \
    map_gnis.c \
    map_OSM.c map_OSM.h \
    map_pop.c \
    map_shp.c map_shp_fwd.h \
    map_tif.c \
    map_WMS.c \
    messages.c messages.h \
    messages_gui.c \
    mgrs_utils.c mgrs_utils.h \
    mutex_utils.c mutex_utils.h \
    objects.h objects.c \
    objects_gui.c objects_gui.h \
    object_utils.h object_utils.c \
    popup.h \
    popup_gui.c \
    rac_data.c rac_data.h \
    rotated.c rotated.h \
    rpl_malloc.c rpl_malloc.h \
    shp_hash.c shp_hash.h \
    snprintf.c snprintf.h \
    sound.c sound.h symbols.h \
    tactical_call_utils.c tactical_call_utils.h \
    tile_mgmnt.c tile_mgmnt.h \
    timer_utils.c timer_utils.h \
    track_gui.c track_gui.h \
    util.c util.h \
    view_message_gui.c \
    wx.c wx.h \
    wx_gui.c \
    x_spider.c x_spider.h \
    xa_config.c xa_config.h \
    xastir.h

# Conditionally compiled sources
if HAVE_NOMINATIM
NOMINATIM_SRC = nominatim.c nominatim.h
else
NOMINATIM_SRC =
endif

xastir_SOURCES = $(XASTIR_SRC) $(NOMINATIM_SRC) compiledate.c

xastir_udp_client_SOURCES = \
    xastir_udp_client.c

testdbfawk_SOURCES = \
    testdbfawk.c \
    awk.c \
    dbfawk.c \
    rpl_malloc.c rpl_malloc.h

.PHONY: compiledate.c

compiledate.c: $(XASTIR_SRC)
	rm -f compiledate.c compiledate.o
	echo 'char gitstring[] = "'"`$(top_srcdir)/scripts/XastirGitStamp.sh $(top_srcdir)`"'";' >> compiledate.c

remove_compiledate:
	rm -f compiledate.c compiledate.o

xastir_LDADD=-Lrtree -lrtree

xastir_LINK=$(CC) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@

xastir_udp_client_LINK=$(CC) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@

testdbfawk_LINK=$(CC) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@
