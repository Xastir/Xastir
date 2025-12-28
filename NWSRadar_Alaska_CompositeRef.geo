WMSSERVER
#
# The National Weather Service has several WMS services to provide
# weather radar images.  Information about the various products available
# can be had at https://opengeo.ncep.noaa.gov/geoserver/www/index.html
#
# This file allows the Quality Controlled Composite Reflectivity map
# for Alaska

URL https://opengeo.ncep.noaa.gov/geoserver/alaska/alaska_cref_qcd/ows?service=wms&version=1.1.1&request=GetMap&SRS=EPSG:4326&LAYERS=alaska_cref_qcd&STYLE=&FORMAT=image/png&TRANSPARENT=TRUE

TRANSPARENT 0xFFFFFF
REFRESH 600


