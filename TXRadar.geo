#
# $Id$
#
#US Composite Radar image (Unidata/LDM/Gempak) n5jxs 2003 08 25
URL     http://mesonet.tamu.edu/gemdata/images/radar/01_mesonet.png
#                       X               Y               Long			Lat
TIEPOINT                200             200             -104.80000		35.50000
TIEPOINT                1130            1090            -95.50000		26.60000
# Image extents: Lat: 24.6N to 37.5N, Lon: 93.5W to 106.8W (-93.5 to -106.8) # Image size extents: X: 1330 pixels, Y: 1290 pixels (.01 deg/pixel)
IMAGESIZE 1330 1290
#
# REFRESH tells your program just how often to retrieve the radar
# image.  Images are recreated on the server every 6 minutes (720 sec).
REFRESH 720
# Transparent tells the program and image handling software what color
#is to be considered transparent.  In this case, it's white and valid
# for a 24-bit color map.
#TRANSPARENT 0xffffff
# The following should work for a 16-bit color map.
#TRANSPARENT 0x0ffff
# The following should work for all color maps, now.
TRANSPARENT 0x0ffffffff

