#!/bin/sh +x
#
#
# Script to retrieve and install Shapelib. 
#
# Written 20050227 Dan Brown N8YSZ
#
# Copyright (C) 2000-2005  The Xastir Group
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

SHAPELIB=shapelib-1.2.10

SHAPELIB_FILE=${SHAPELIB}.tar.gz

SHAPELIB_URL=http://dl.maptools.org/dl/shapelib/${SHAPELIB_FILE}

XASDIR=$HOME/src/xastir
XASTIR_TMP=$XASDIR/tmp

XASTIR_LIB=$HOME/src/xastir/lib


UNAME=`uname -o`

if [ $UNAME = 'Cygwin' ]
then
    printf "Os is %s. Skipping sudo tests.\n" $UNAME 
else 
    printf "Checking for sudo\n" 

    if  SUDO=`which sudo` 
    then 
        printf "$SUDO found - validating $SUDO privilages\n" 
        if $SUDO -v 
        then
            printf "Ok, we can continue\n"
        else
            printf "ERROR: %s needs $SUDO privilages - aborting \n" $0
            exit 
        fi 
    else

#
# The following isn't very portable and should be modified. 
#
        printf "Sudo not found. Checking for appropriate privs\n" 
        if touch -a /etc/ld.so.conf 
        then
            printf "We can modify /etc/ld.so.conf\n"
        else
            printf "ERROR: We cannot modify /etc/ld.so.conf - aborting\n"
            exit 
        fi 
        if touch -a /usr/local/lib 
        then
            printf "We can modify /usr/local/lib\n"
        else
            printf "ERROR: We cannot modify /usr/local/lib - aborting\n"
            exit
        fi 
    fi 
fi 

if [ ! -d $XASTIR_TMP ]
then
    printf "ERROR: %s Doesn't appear to exsit.\n" $XASTIR_TMP
    printf "Please create dir and/or edit script. Exiting\n"
    exit 
else 
    cd $XASTIR_TMP
fi

if [ -e $SHAPELIB_FILE -o -e $SHAPELIB ]
then

    printf "cleaning up old shapelib - will be saved under dir: %s \n" old.$$
    mkdir old.$$
    mv -f shapelib* old.$$/
fi 

printf "Retrieving shapelib\n"

if (wget $SHAPELIB_URL)
then
    if (tar -xzf $SHAPELIB_FILE )
    then 
        printf "shapelib successfully downloaded.\n" 
    else 
        printf "ERROR: %s not successfully downloaded - aborting.\n" $SHAPELIB_FILE
    fi 
fi 

printf "Building shapelib\n" 
cd $SHAPELIB

# Need a couple fixes for shapelib on Cygwin. 
if [ $UNAME = 'Cygwin' ]
then
    mv Makefile Makefile.dist
    sed -e "s/h libshp.so/hlibshp.sl/" -e "s/-lc/-lcygwin/"  < Makefile.dist > Makefile 
fi 

make 
make lib

printf "Attempting install\n"
printf "\n----------------------------------------------------------------------\n"
if $SUDO make lib_install
then
    printf "\n\n\n----------------------------------------------------------------------\n"
else
    printf "\n\n\n----------------------------------------------------------------------\n"
    printf "Error: Install appears to have failed - aborting \n"
fi 


if [ $UNAME = 'Cygwin' ]
then
    printf "OS is %s - Skipping ldconfig " $UNAME
else

    printf "Checking /etc/ld.so.conf"

    if (! grep /usr/local/lib /etc/ld.so.conf 2>&1 > /dev/null) 
    then
        printf "Warning: /usr/local/lib not in /etc/ld.so.conf - adding it\n"
        cp /etc/ld.so.conf /tmp
        printf "/usr/local/lib\n" >> /tmp/ld.so.conf
        $SUDO cp /etc/ld.so.conf /etc/ld.so.conf.save 
        $SUDO cp /tmp/ld.so.conf /etc/ld.so.conf
    fi

    if ( grep /usr/local/lib /etc/ld.so.conf ) 
    then
        printf "Running ldconfig\n" 
        if ($SUDO ldconfig ) 
        then
            printf "ldconfig completed successfully\n\n" 
        else
            printf "ldconfig had errors - you may need to run ldconfig manually.\n" 
        fi
    else
        printf "ERROR: could not add /usr/local/lib to /etc/ld.so.conf - aborting\n"
        exit
    fi
fi

printf "Congratulations, %s is done. \n" $0
printf "If you got no errors, shapelib should now be installed!!\n"
printf "If there are errors, please see: \n"
printf "\t%s\n" ${XASTIR_TMP}/${SHAPELIB}/README
printf "\t%s\n" ${XASDIR}/README.MAPS


