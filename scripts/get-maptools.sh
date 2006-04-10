#!/bin/bash +x
#
# $Id$
#
#
# Script originally to retrieve and install Shapelib. 
# Written 20050227 Dan Brown N8YSZ
# Modified 20060321 to generalize for all maptools - N8YSZ. 
#
#
# Copyright (C) 2000-2006  The Xastir Group
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
# 02111-1307, USA.
#
# Look at the README for more information on the program.
#

# uncomment as appropriate. You'll want gnu tar
#TAR=/usr/local/bin/tar 
TAR=/bin/tar

MAPTOOLS=http://dl.maptools.org/dl

# 
XASDIR=$HOME/src/xastir
XASTIR_TMP=$XASDIR/tmp
XASTIR_LIB=$XASDIR/lib



printf "Checking MACHTYPE found: %s\n" $MACHTYPE

if [ ${MACHTYPE}.z = '.z' ]
then
    printf "You really ought to upgrade to a current version of bash \n"
    printf "Making best guess attempt using uname\n"
    UNAME=`uname`
else
    UNAME=$MACHTYPE
fi

if [ -e /sbin/ldconfig ]
then
    printf "OS is %s. Found ldconfig.\n" $UNAME 	 
    DO_LDCONFIG='yes'
else
    printf "OS is %s. Skipping ldconfig.\n" $UNAME 
    DO_LDCONFIG='no'
fi

printf "Checking for sudo\n" 

if  SUDO=`which sudo` 
then 
    printf "$SUDO found - validating $SUDO privileges\n" 
    if $SUDO -v 
    then
        printf "Ok, we can continue\n"
    else
        printf "ERROR: %s needs $SUDO privileges - aborting \n" $0
        exit 
    fi 
else
    printf "Sudo not found. Checking for appropriate privs\n" 
    if [ $DO_LD_CONFIG="yes" -a -f /etc/ld.so.conf ]
    then
        if  touch -a /etc/ld.so.conf 
        then
            printf "We can modify /etc/ld.so.conf\n"
        else
            printf "ERROR: We cannot modify /etc/ld.so.conf - aborting\n"
            exit 
	fi 
    fi

    if touch -a /usr/local/lib 
    then
        printf "We can modify /usr/local/lib\n"
    else
        printf "ERROR: We cannot modify /usr/local/lib - aborting\n"
        exit
    fi 
fi 

if [ $DO_LDCONFIG = "no" ]
then
    printf "OS is %s - Skipping ldconfig \n" $UNAME
else

    printf "Checking /etc/ld.so.conf configuration\n"

    if [ -d /etc/ld.so.conf.d ]
    then 
	LDCONF_FILE=/etc/ld.so.conf.d/xastir.conf 
    else
	LDCONF_FILE=/etc/ld.so.conf
    fi

    if [ ! -f $LDCONf_FILE ]
    then 
	sudo touch $LDCONF_FILE
    fi

    if (! grep /usr/local/lib $LDCONF_FILE 2>&1 > /dev/null) 
    then
        printf "Warning: /usr/local/lib not in %s - adding it\n" $LDCONF_FILE
	if [ -f $LDCONF_FILE ]
	then
	        cp $LDCONF_FILE /tmp/ldconfig.tmp
	        $SUDO cp $LDCONF_FILE $LDCONF_FILE.orig.$$
	fi
        printf "/usr/local/lib\n" >> /tmp/ldconf.tmp
        $SUDO cp /tmp/ldconf.tmp $LDCONF_FILE

    fi

    if (! grep /usr/local/lib $LDCONF_FILE 2>&1 > /dev/null) 
    then
        printf "ERROR: could not add /usr/local/lib to %s - aborting\n " $LDCONFIG_FILE
        exit
    fi

fi


# pcre arguably doesn't belong here

ALL="	http://internap.dl.sourceforge.net/sourceforge/pcre/pcre-6.3.tar.gz
	http://dl.maptools.org/dl/shapelib/shapelib-1.2.10.tar.gz\
	http://dl.maptools.org/dl/proj/proj-4.4.9.tar.gz\
	http://dl.maptools.org/dl/gdal/gdal-1.3.1.tar.gz\
	http://dl.maptools.org/dl/geotiff/libgeotiff/libgeotiff-1.2.3.tar.gz"

#for XA_LIB in shapelib-1.2.10 proj-4.4.9 gdal-1.3.1 libgeotiff-1.2.3

for XA_LIB_URL in $ALL
do 


# Check for a working dir

	if [ ! -d $XASTIR_TMP ]
	then
	    printf "ERROR: %s Doesn't appear to exist.\n" $XASTIR_TMP
	    printf "Please create dir and/or edit script. Exiting\n"
	    exit 
	else 
	    cd $XASTIR_TMP
	fi


# 	PACKAGE_DIR=`echo $XA_LIB | sed -e "s/\-.*//"`

	XA_LIB_FILE=`echo $XA_LIB_URL | sed -e "s/.*\///g"`
#	XA_LIB_URL=${MAPTOOLS}/${PACKAGE_DIR}/${XA_LIB_FILE}
	XA_LIB=`echo $XA_LIB_FILE | sed -e "s/.tar.gz//"`
	printf "Working on: %s\n" $XA_LIB
	printf "Working in: %s\n" `pwd`
	

# Cleanup Leftovers 

	if [ -e $XA_LIB_FILE -o -e $XA_LIB ]
	then
	
	    printf "cleaning up old %s - will be saved under dir: %s \n" $XA_LIB old.$$
	    mkdir old.$$
	    mv -f ${XA_LIB}* old.$$/
	fi 

# Get Files from Maptools 

	printf "Retrieving: %s\n" $XA_LIB

	if (wget $XA_LIB_URL)
	then
	    if ($TAR -xzf $XA_LIB_FILE )
	    then 
	        printf "%s successfully downloaded.\n" $XA_LIB_FILE
	    else 
	        printf "ERROR: %s not successfully downloaded - skipping.\n" $XA_LIB_FILE
	    fi 
	fi 

	printf "Building %s\n\n" $XA_LIB
	cd $XA_LIB
	printf "Working in: %s\n" `pwd`

	# Standards and methods and packaging - so many to chose from!

	if ( printf $XA_LIB |grep -i proj) 
	then
		cd nad
		XA_LIB_URL=${MAPTOOLS}/proj/proj-datumgrid-1.3.zip
		wget $XA_LIB_URL
		unzip proj-datumgrid-1.3.zip
		cd $XASTIR_TMP/$XA_LIB 
	fi

	if [ $XA_LIB = 'shapelib-1.2.10' ] 
	then
	# Need a couple fixes for shapelib on Cygwin. 

		if (echo $UNAME |grep -i cygwin ) 2>&1 
		then
		    mv Makefile Makefile.dist
		    sed -e "s/h libshp.so/hlibshp.sl/" -e "s/-lc/-lcygwin/"  < Makefile.dist > Makefile 
		fi 
		make 
		make lib
	else

		./configure
		make 
	fi  2>&1 >>${XA_LIB}.build.$$

	printf "\n----------------------------------------------------------------------\n"
	printf "Attempting install of %s\n" ${XA_LIB}

	if [ ${XA_LIB} = 'shapelib-1.2.10' ] 
	then
		$SUDO make lib_install
	else
		$SUDO make install
	fi 2>&1 >> ${XA_LIB}.install.$$

        printf "\n----------------------------------------------------------------------\n"
	if $!
	then

	    printf "If you got no errors(*), %s should now be installed!!\n" $XA_LIB
	    printf "\t(* warnings should be OK)\n"

	else

	    printf "Error: Install of %s appears to have failed \n" $XA_LIB
	fi 

        printf "If there are errors or warnings, please see: \n"
        printf "\t%s\n" ${XASTIR_TMP}/${XA_LIB}/README
        printf "\t%s\n" ${XASTIR_TMP}/${XA_LIB}/${XA_LIB}.build.$$
        printf "\t%s\n" ${XASTIR_TMP}/${XA_LIB}/${XA_LIB}.install.$$
        printf "\n----------------------------------------------------------------------\n"



	if [ $DO_LDCONFIG = "no" ]
	then
	    printf "OS is %s - Skipping ldconfig \n" $UNAME
	else
	    printf "Running ldconfig\n" 
	    if ($SUDO ldconfig ) 
	    then
	        printf "ldconfig completed successfully\n\n" 
	    else
	        printf "ldconfig had errors - you may need to run ldconfig manually.\n" 
	    fi
	fi

done     # with getting and building


printf "Congratulations, %s is done. \n" $0
printf "For more information see %s\n" ${XASDIR}/README.MAPS


