# $Id$
#
# Copyright (C) 2000-2005  The Xastir Group
#

bin_PROGRAMS = xastir udp_client
noinst_PROGRAMS = testawk

SUBDIRS = @SUBRTREE@
DIST_SUBDIRS = rtree

xastir_SOURCES = \
    alert.c alert.h \
    awk.c awk.h \
    bulletin_gui.c bulletin_gui.h \
    color.c color.h \
    datum.c datum.h \
    db.c database.h \
    dbfawk.c dbfawk.h \
    draw_symbols.c draw_symbols.h \
    fcc_data.c fcc_data.h \
    festival.c festival.h \
    geo-find.c geo.h \
    geocoder_gui.c \
    gps.c gps.h \
    hashtable.c hashtable_private.h hashtable.h \
    hashtable_itr.c hashtable_itr.h \
    hostname.c hostname.h \
    igate.c igate.h \
    interface.c interface.h \
    interface_gui.c \
    io-common.c io-mmap.c io.h \
    lang.c lang.h \
    leak_detection.h \
    list_gui.c list_gui.h \
    locate_gui.c \
    location.c \
    location_gui.c \
    main.c main.h \
    maps.c maps.h \
    map_cache.c map_cache.h \
    map_dos.c \
    map_geo.c \
    map_gdal.c \
    map_gnis.c \
    map_pdb.c \
    map_shp.c \
    map_tif.c \
    map_tiger.c \
    map_WMS.c \
    messages.c messages.h \
    messages_gui.c \
    popup.h \
    popup_gui.c \
    rac_data.c rac_data.h \
    rotated.c rotated.h \
    snprintf.c snprintf.h \
    sound.c symbols.h \
    shp_hash.c shp_hash.h \
    track_gui.c track_gui.h \
    util.c util.h \
    view_message_gui.c \
    wx.c wx.h \
    wx_gui.c \
    xa_config.c xa_config.h \
    xastir.h \
    x_spider.c x_spider.h

udp_client_SOURCES = \
    udp_client.c
 
testawk_SOURCES = \
    testawk.c \
    awk.c \
    dbfawk.c

xastir_LDADD=@LIBRTREE@

xastir_LINK=$(CC) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@

udp_client_LINK=$(CC) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@

testawk_LINK=$(CC) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@

