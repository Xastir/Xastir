#!/usr/bin/env python
###############################################################################
# $Id$
#
# Portions Copyright (C) 2004-2005  The Xastir Group
#
# Modified version of GDAL/OGR "tigerpoly.py" script (as described below)
# adapted to assemble information from more tables of the TIGER/Line data
# than had been done by the original
# You must have installed GDAL/OGR, configured to use python in order to use 
# this script
###############################################################################
# 
# Adapted for Xastir use by Tom Russo
#
####################Original comments follow
###############################################################################
# tigerpoly.py,v 1.3 2003/07/11 14:52:13 warmerda Exp 
#
# Project:  OGR Python samples
# Purpose:  Assemble TIGER Polygons.
# Author:   Frank Warmerdam, warmerdam@pobox.com
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# 
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
###############################################################################


import osr
import ogr
import string
import sys

#############################################################################
class Module:

    def __init__( self ):
        self.lines = {}
        self.poly_line_links = {}

#############################################################################
# I dunno, does this initializer make sense?
class Field:
    def __init__( self ):
        self.layer = ''
        self.layerName = ''
        self.field_defn = ''
        self.fld_index = ''

#############################################################################
def Usage():
    print 'Usage: tigerpoly.py infile [outfile].shp'
    print
    sys.exit(1)

#############################################################################
# Argument processing.

infile = None
outfile = None

i = 1
while i < len(sys.argv):
    arg = sys.argv[i]

    if infile is None:
	infile = arg

    elif outfile is None:
	outfile = arg

    else:
	Usage()

    i = i + 1

if outfile is None:
    outfile = 'poly.shp'

if infile is None:
    Usage()

#############################################################################
# Open the datasource to operate on.

ds = ogr.Open( infile, update = 0 )

poly_layer = ds.GetLayerByName( 'Polygon' )
pip_layer = ds.GetLayerByName( 'PIP' )
areaLandmarks_layer = ds.GetLayerByName( 'AreaLandmarks' )
Landmarks_layer = ds.GetLayerByName( 'Landmarks' )

#############################################################################
#	Create output file for the composed polygons.

shp_driver = ogr.GetDriverByName( 'ESRI Shapefile' )
shp_driver.DeleteDataSource( outfile )

shp_ds = shp_driver.CreateDataSource( outfile )

shp_layer = shp_ds.CreateLayer( 'out', geom_type = ogr.wkbPolygon )

# Now create a list of all the fields for the DBF file, using the unique
# fields in Polygon, PIP, AreaLandmarks and Landmarks --- eliminate
# all fields of identical names (because those are just the fields that
# link the records to each other).  When we do so, must store layer, field
# index in the hash, and we'll write it back by 

# Create a hash of field definitions indexed by field name.
fields_hash = {}
field_names = []

src_defn = poly_layer.GetLayerDefn()
poly_field_count = src_defn.GetFieldCount()

for fld_index in range(poly_field_count):
    src_fd = src_defn.GetFieldDefn( fld_index )
    fields_hash[src_fd.GetName()] = Field()
    fields_hash[src_fd.GetName()].layer=poly_layer
    fields_hash[src_fd.GetName()].layerName='Polygon'
    fields_hash[src_fd.GetName()].field_defn=src_defn
    fields_hash[src_fd.GetName()].fld_index=fld_index
    field_names.append(src_fd.GetName())
#    print 'Got a Polygon field called %s' % src_fd.GetName()


# now loop over other layers
src_defn = pip_layer.GetLayerDefn()
pip_field_count = src_defn.GetFieldCount()
for fld_index in range(pip_field_count):
    src_fd = src_defn.GetFieldDefn( fld_index )
    try:
      foo = fields_hash[src_fd.GetName()]
#      print ' found pip field %s already in hash' % src_fd.GetName()
    except:
      field=Field()
      fields_hash[src_fd.GetName()] = field
      fields_hash[src_fd.GetName()].layer=pip_layer
      fields_hash[src_fd.GetName()].layerName='PIP'
      fields_hash[src_fd.GetName()].field_defn=src_defn
      fields_hash[src_fd.GetName()].fld_index=fld_index
      field_names.append(src_fd.GetName())
#      print 'Got a pip field called %s' % src_fd.GetName()            

src_defn = areaLandmarks_layer.GetLayerDefn()
areaLandmarks_field_count = src_defn.GetFieldCount()
for fld_index in range(areaLandmarks_field_count):
    src_fd = src_defn.GetFieldDefn( fld_index )
    try:
      foo = fields_hash[src_fd.GetName()]
#      print ' found areaLandmarks field %s already in hash' % src_fd.GetName()
    except:
      field=Field()
      fields_hash[src_fd.GetName()] = field
      fields_hash[src_fd.GetName()].layer=areaLandmarks_layer
      fields_hash[src_fd.GetName()].layerName='AreaLandmarks'
      fields_hash[src_fd.GetName()].field_defn=src_defn
      fields_hash[src_fd.GetName()].fld_index=fld_index
      field_names.append(src_fd.GetName())
#      print 'Got a areaLandmarks field called %s' % src_fd.GetName()            
src_defn = Landmarks_layer.GetLayerDefn()
Landmarks_field_count = src_defn.GetFieldCount()
for fld_index in range(Landmarks_field_count):
    src_fd = src_defn.GetFieldDefn( fld_index )
    try:
      foo = fields_hash[src_fd.GetName()]
#      print ' found Landmarks field %s already in hash' % src_fd.GetName()
    except:
      field=Field()
      fields_hash[src_fd.GetName()] = field
      fields_hash[src_fd.GetName()].layer=Landmarks_layer
      fields_hash[src_fd.GetName()].layerName='Landmarks'
      fields_hash[src_fd.GetName()].field_defn=src_defn
      fields_hash[src_fd.GetName()].fld_index=fld_index
      field_names.append(src_fd.GetName())
#      print 'Got a Landmarks field called %s' % src_fd.GetName()            

# we now have a hash whose keys are all our field names, and from which
# we *should* be able to retreive fields as needed
#print ' we found %d fields' % len(fields_hash)
#print ' the names array has %d names ' % len(field_names)
#for field_name in (field_names):
#  print ' key %s we get field from %s ' % (field_name ,fields_hash[field_name].layerName)

# Now loop over all those field names, create the dbf file definition

for field_name in (field_names):
  src_defn = fields_hash[field_name].field_defn
  src_fd = src_defn.GetFieldDefn( fields_hash[field_name].fld_index )
  fd = ogr.FieldDefn( src_fd.GetName(), src_fd.GetType() )
  fd.SetWidth( src_fd.GetWidth() )
  fd.SetPrecision( src_fd.GetPrecision() )
  shp_layer.CreateField(fd)

#############################################################################
# Read all features in the line layer, holding just the geometry in a hash
# for fast lookup by TLID.

line_layer = ds.GetLayerByName( 'CompleteChain' )
line_count = 0

modules_hash = {}

feat = line_layer.GetNextFeature()
geom_id_field = feat.GetFieldIndex( 'TLID' )
tile_ref_field = feat.GetFieldIndex( 'MODULE' )
while feat is not None:
    geom_id = feat.GetField( geom_id_field )
    tile_ref = feat.GetField( tile_ref_field )

    try:
        module = modules_hash[tile_ref]
    except:
        module = Module()
        modules_hash[tile_ref] = module

    module.lines[geom_id] = feat.GetGeometryRef().Clone()
    line_count = line_count + 1

    feat.Destroy()

    feat = line_layer.GetNextFeature()

print 'Got %d lines in %d modules.' % (line_count,len(modules_hash))

#############################################################################
# Read all polygon/chain links and build a hash keyed by POLY_ID listing
# the chains (by TLID) attached to it. 

link_layer = ds.GetLayerByName( 'PolyChainLink' )

feat = link_layer.GetNextFeature()
geom_id_field = feat.GetFieldIndex( 'TLID' )
tile_ref_field = feat.GetFieldIndex( 'MODULE' )
lpoly_field = feat.GetFieldIndex( 'POLYIDL' )
rpoly_field = feat.GetFieldIndex( 'POLYIDR' )

link_count = 0

while feat is not None:
    module = modules_hash[feat.GetField( tile_ref_field )]

    tlid = feat.GetField( geom_id_field )

    lpoly_id = feat.GetField( lpoly_field )
    rpoly_id = feat.GetField( rpoly_field )

    try:
        module.poly_line_links[lpoly_id].append( tlid )
    except:
        module.poly_line_links[lpoly_id] = [ tlid ]

    try:
        module.poly_line_links[rpoly_id].append( tlid )
    except:
        module.poly_line_links[rpoly_id] = [ tlid ]

    link_count = link_count + 1

    feat.Destroy()

    feat = link_layer.GetNextFeature()

print 'Processed %d links.' % link_count

# Now we need to pull in all the PIP, AreaLandmarks and Landmarks features
# and keep them in a hash based on POLYID

#PIP:
feat = pip_layer.GetNextFeature()
polyid_field = feat.GetFieldIndex( 'POLYID' )
pip_hash={}

while feat is not None:
   poly_id = feat.GetField( polyid_field )
   pip_hash[poly_id] = feat
   feat = pip_layer.GetNextFeature()   

print 'Processed %d PIP records.' % len(pip_hash)

#AreaLandmarks
feat = areaLandmarks_layer.GetNextFeature()
polyid_field = feat.GetFieldIndex( 'POLYID' )
areaLandmarks_hash={}

while feat is not None:
   poly_id = feat.GetField( polyid_field )
   areaLandmarks_hash[poly_id] = feat
   feat = areaLandmarks_layer.GetNextFeature()   
print 'Processed %d AreaLandmarks records.' % len(areaLandmarks_hash)


# Landmarks is not looked up by polyid, but by LAND, which links it to 
# Landmarks
feat = Landmarks_layer.GetNextFeature()
land_field = feat.GetFieldIndex( 'LAND' )
Landmarks_hash={}

while feat is not None:
   land_id = feat.GetField( land_field )
   Landmarks_hash[land_id] = feat
   feat = Landmarks_layer.GetNextFeature()   
print 'Processed %d Landmarks records.' % len(Landmarks_hash)


# Boy, what a mess.  But we now have all the data we need in hashes, so we
# can loop over POLYGON records and extract data as we need it.
# Do that now:
#############################################################################
# Process all polygon features.

feat = poly_layer.GetNextFeature()
tile_ref_field = feat.GetFieldIndex( 'MODULE' )
polyid_field = feat.GetFieldIndex( 'POLYID' )

poly_count = 0

while feat is not None:
    module = modules_hash[feat.GetField( tile_ref_field )]
    polyid = feat.GetField( polyid_field )

    tlid_list = module.poly_line_links[polyid]

    link_coll = ogr.Geometry( type = ogr.wkbGeometryCollection )
    for tlid in tlid_list:
        geom = module.lines[tlid]
        link_coll.AddGeometry( geom )

    try:
        poly = ogr.BuildPolygonFromEdges( link_coll )

        #print poly.ExportToWkt()
        #feat.SetGeometryDirectly( poly )

        feat2 = ogr.Feature(feature_def=shp_layer.GetLayerDefn())

        for fld_index in range(len(field_names)):
           theFieldName = field_names[fld_index]
           layerName = fields_hash[theFieldName].layerName
           theFieldIdx = fields_hash[theFieldName].fld_index
#           print 'fetching field %s from layer %s' % (theFieldName, layerName)
           # if it's from Polygon just pop it in because we have it now:
           if layerName == 'Polygon':
              feat2.SetField( fld_index, feat.GetField(theFieldIdx) )
           # If it's from PIP, we definitely have one
           elif layerName == 'PIP':
              feat2.SetField(fld_index, pip_hash[polyid].GetField(theFieldIdx))
           # this could be a problem, coz there might not be one
           elif layerName == 'AreaLandmarks':
              try:
                feat3 = areaLandmarks_hash[polyid]
#                print ' found feature with polyid %d in AreaLandmarks' % polyid
                feat2.SetField(fld_index, feat3.GetField(theFieldIdx))
              except:
                feat2.UnsetField(fld_index)
           # this one's mega tricky, coz it depends on there being
           # an AreaLandmarks first
           elif layerName == 'Landmarks':
              try:
                feat3 = areaLandmarks_hash[polyid]
#                print ' found feature with polyid %d in AreaLandmarks' % polyid
                landidx1 = feat3.GetFieldIndex('LAND')
#                print '  LAND is field %d in AreaLandmarks' % landidx1
#                print '   LAND field in this record is %d.' % feat3.GetField(landidx1)
                feat4 = Landmarks_hash[feat3.GetField(landidx1)]
#                print ' found feature with LAND %d in Landmarks' % feat3.GetField(landidx1)
                feat2.SetField(fld_index, feat4.GetField(theFieldIdx))
              except:
                feat2.UnsetField(fld_index)
           else:
              print 'unknown layer %s referenced.' % layerName

        feat2.SetGeometryDirectly( poly )

        shp_layer.CreateFeature( feat2 )
        feat2.Destroy()

        poly_count = poly_count + 1
    except:
        print 'BuildPolygonFromEdges failed.'

    feat.Destroy()

    feat = poly_layer.GetNextFeature()

print 'Built %d polygons.' % poly_count
           

#############################################################################
# Cleanup

shp_ds.Destroy()
ds.Destroy()
