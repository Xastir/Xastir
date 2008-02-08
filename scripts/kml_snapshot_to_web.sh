#!/bin/bash

# This shell script copies Xastir snapshots from the directory in which 
# they are created ~/.xastir/temp to a directory where a web server can deliver
# them as a kml feed to overlay the current snapshot from on the terrain in a kml
# capable application that is subscribed to the feed.
#
# You will need to set two parameters for this script: 
# 1. set DIR= to the directory into which you wish to copy the snapshot files.
# This can be a directory on a remote webserver mounted using sshfs.
# 2. change www.example.com to the address of your webserver.  You will also
# need to change the link specified in kml_snapshot_feed.kml
# 
# You can copy the kml_snapshot_feed.kml into the web folder, and load the
# http://www.example.com/tracks/kml_snapshot_feed.kml file into your KML 
# application instead of http://www.example.com/tracks/snapshot.kml, and run 
# this shell script with cron to periodically update the snapshot.kml file.  
# This should enable your KML application to refresh the snapshots in sync
# with their creation by Xastir.
#
# Note: GE will load jpg files but not png files, so ImageMagick's 
# convert is used here to convert the snapshot.png produced by Xastir to 
# a snapshot.jpg file.
#
DIR=/var/www/htdocs/tracks
cd ~/.xastir/tmp
convert ./snapshot.png $DIR/snapshot.jpg
cat snapshot.kml | gawk  -- '  { gsub(/<href>/,"&http://www.example.com/tracks/") } { gsub(/snapshot.png/,"snapshot.jpg") } { print } ' > $DIR/snapshot.kml
