#
# $Id$
#
#
# Map data Copyright OpenStreetMap Contributors, CC-BY-SA
# See www.openstreetmap.org and http://creativecommons.org/licenses/by-sa/2.0/
#
# The string following OSMSTATICMAP-, is appended to the URL
# The string is typically expected to select a layer, and possibly style
# options. If not set it defaults to:
#   layer=osmarander
#
OSMSTATICMAP-layer=cloudmade_998

# Uncomment this URL to override the default.
#URL http://ojw.dev.openstreetmap.org/StaticMap/

# When defined:
# OSM_OPTIMIZE_KEY will change the map scaling to the nearest OSM zoom level.
# OSM_REPORT_SCALE_KEY will report the present, scale_x, scale_y,
#   and OSM zoom level, but only for debug level 512 (-v 512)
#
# The values are X KeySym values.
# 65473 = F4
# 65474 = F5
OSM_OPTIMIZE_KEY 65473
OSM_REPORT_SCALE_KEY 65474

