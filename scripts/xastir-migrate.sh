#!/bin/sh
# migrate from old xastir USER FILES to new directory structure.
# Stuff that was under /usr/local/xastir is now under /usr/local/share/xastir/
# fixed up the .xastir/config/xastir.cfg as well.
# XXX Do we throw away the xastir non-user files (config, doc, help)?
PREFIX=
OLD=$PREFIX/usr/local/xastir
NEW=$PREFIX/usr/local/share/xastir
rr=0
if [ -d $OLD ]; then
        if [ ! -d $NEW ]; then
	    (umask 022; mkdir -p $NEW)
	fi
	echo "Moving xastir user files"
	echo "From: $OLD"
	echo "  To: $NEW"
	for i in Counties fcc GNIS maps sounds
        do
	    if [ -d $OLD/$i ]; then
		if [ -d $NEW/$i ]; then
		    echo "$NEW/$i: destination directory already exists;  Overlaying files."
		    (cd $OLD/$i; tar cf - .) | (cd $NEW/$i; tar xvfp -)
		    r=$?
		    if [ $r -ne 0 ]; then
			echo Failed to move directory $i
			rr=`expr $rr + $r`
		    else
			# clean up the source directory
			rm -rf $NEW/$i
		    fi
		else
		    # hope they are in the same FS!
		    mv $OLD/$i $NEW/$i
		    r=$?
		    rr=`expr $rr + $r`
		    if [ $r -ne 0 ]; then
			echo Failed to move directory $i
		    fi
		fi
	    else
		echo "$OLD/$i: old directory not found."
	    fi
	done
else
        echo "$OLD: not found.  Nothing to migrate."
fi
if [ $rr -ne 0 ]; then
    echo "WARNING: Some files/directories not moved.  Please check $OLD and $NEW"
else
    echo "No Errors"
fi
exit $rr
