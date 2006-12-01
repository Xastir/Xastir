#
# $Id$
#
#US Composite Radar image (Unidata/LDM/Gempak) n5jxs 2003 08 25
# Modified for accuracy, comments:  n5jxs 2004 03 15 1400UTC
#
# If you want to get a different image-type, change the selection
# below by removing the '#' from the front of the URL, and placing
# a '#' before all URLs for image-types you don't want.
# I don't know what will happen if you have multiple URLs selected.
URL     http://mesonet.tamu.edu/gemdata/images/radar/01_USrad.png
#URL     http://mesonet.tamu.edu/gemdata/images/radar/01_USrad.gif
#URL     http://mesonet.tamu.edu/gemdata/images/radar/01_USrad.jpg
#URL     http://mesonet.tamu.edu/gemdata/images/radar/01_USrad.tif
#
#
#           X       Y       Long        Lat
TIEPOINT    200     200     -123.00000  48.00000
TIEPOINT    5999    2499    -65.00000   23.00000
# Image extents: Lat: 23.0N to 50.0N, Lon: 65.0W to 125.0W (-65.0 to -125.0)
# Image size extents: X: 6000 pixels, Y: 2650 pixels (.01 deg/pixel)
IMAGESIZE 6000 2500
#
# REFRESH tells your program just how often to retrieve the radar
# image.  Images are recreated on the server every 6 minutes (720
# sec).
REFRESH 720
# Transparent tells the program and image handling software what
# color is to be considered transparent.  In this case, it's white
# and valid for a 24-bit color map.
#TRANSPARENT 0xffffff
# The following should work for a 16-bit color map.
#TRANSPARENT 0x0ffff
# The following should work for all color maps, now.
#TRANSPARENT 0x0ffffffff
TRANSPARENT 0x000000000

