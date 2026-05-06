WMSSERVER
#
# Census changed layer names from text to numbers.
# these numbers correspond to:
#Primary Roads (24)
#Primary Roads Labels (23)
#Secondary Roads
#Secondary Roads Labels
#Local Roads
#Local Roads Labels
#Railroads
#Railroads Labels
#Linear Hydrography
#Linear Hydrography Labels
#Areal Hydrography
#Areal Hydrography Labels
#Glaciers
#Glaciers Labels
#National Park Service Areas
#National Park Service Areas Labels
#Military Installations (2)
#Military Installations Labels (1)
# When in doubt, use the GetCapabilities request to check:
#https://tigerweb.geo.census.gov/arcgis/services/TIGERweb/tigerWMS_PhysicalFeatures/MapServer/WMSServer?VERSION=1.1.1&SERVICE=WMS&REQUEST=GetCapabilities

URL https://tigerweb.geo.census.gov/arcgis/services/TIGERweb/tigerWMS_PhysicalFeatures/MapServer/WMSServer?VERSION=1.1.1&SERVICE=WMS&REQUEST=GetMap&SRS=EPSG:4326&LAYERS=1,2,7,8,10,11,12,13,14,15,17,18,19,20,21,22,23,24&STYLES=&FORMAT=image/png&TRANSPARENT=TRUE

#TRANSPARENT 0x999999


