#!/bin/sh
# fix up a user's .xastir/config/xastir.cnf to rewrite /usr/local/xastir
# to /usr/local/share/xastir.  NOTE: This only works when -prefix=/usr/local!
CNF=.xastir/config/xastir.cnf
cd
if [ -f $CNF ]; then
    mv $CNF $CNF.backup
    if [ $? -ne 0 ]; then
	echo "$CNF: unable to rename!"
	exit 1
    fi
    sed -e 's:/usr/local/xastir/:/usr/local/share/xastir/:' <$CNF.backup >$CNF
    if [ $? -ne 0 ]; then
	echo "$CNF: sed failed!"
	mv $CNF.backup $CNF
	exit 1
    fi
else
    echo "No $CNF to edit"
fi
echo "Done.  Old config file is in $CNF.backup"
exit 0
