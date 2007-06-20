WMSSERVER
#URL http://mesonet.tamu.edu/cgi-bin/CONUSradar?SERVICE=WMS&LAYERS=radar,nwscounties
URL http://mesonet.tamu.edu/cgi-bin/CONUSradar?SERVICE=WMS&LAYERS=radar&FORMAT=image/png&TRANSPARENT=TRUE&CRS=CRS:84&BGCOLOR=0x000000&VERSION=1.1.0
#
#
# REFRESH tells your program just how often to retrieve the radar
# image.  Images are recreated on the server every 6 minutes (720
# sec).
REFRESH 720
#
#
# Black
TRANSPARENT 0x010101
TRANSPARENT 0x000000
# White
TRANSPARENT 0xffffff

