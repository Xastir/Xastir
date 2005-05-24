WMSSERVER
#URL http://mesonet.tamu.edu/cgi-bin/CONUSradar?SERVICE=WMS&LAYERS=radar,nwscounties
URL http://mesonet.tamu.edu/cgi-bin/CONUSradar?SERVICE=WMS&LAYERS=radar&FORMAT=image/png&TRANSPARENT=TRUE&CRS=CRS:84&BGCOLOR=0x000000
#
#
# REFRESH tells your program just how often to retrieve the radar
# image.  Images are recreated on the server every 6 minutes (720
# sec).
REFRESH 720
#
# The TRANSPARENT keyword is not supported yet by map_WMS.c.  The
# functionality needs to be copied from map_geo.c or perhaps
# directly controlled by map_geo.c code.
#TRANSPARENT 0x000000

