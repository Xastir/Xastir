#!/bin/sh

# $Id$

# script created 14-MAR-04 by William Baguhn, kc9asi
# This script is in the public domain.
# Comments or suggestions to kc9asi@arrl.net

# This will take a GNIS datapoint file (typically for a whole state, 8+Mb),
# break it down into smaller chunks (typically for a county, 30-200k)
# it will also throw away the stupid trailing spaces and <CR>'s at EOL.

# My short experiment: the state of wisconsin.
# Started with a 12.5Mb file.
# ended with 93 files, totaling 6.7Mb.
# and, the data files run a whole lot faster, especially when zoomed in.

test -e $1 || (echo Try calling $0 with a file as an argument.; exit)

# field 4 isn't just counties, but it's an acceptable label
# as it's usually counties
cut -f4 -d, <$1 >$1.counties

# remove duplicates (sort, uniq)
# the cut here gets rid of any "quirks" because of commas that came earlier
# than were expected, as cut doesn't recognize quoting depths
sort <$1.counties | uniq | cut -f2 -d\" >$1.counties.uniq

# now we want to replace spaces with periods, so that counties with
# spaces in their names work appropriately both for grep and filenaming
tr " " . <$1.counties.uniq >$1.counties
rm $1.counties.uniq

# OK, now we should have a file with a list of the various divisions
# SO, split each one apart

# the regexp for grep assures that we just get "county" and not county,
# hopefully this will make more sensible breaks as county names are
# sometimes found in other names as well.
# (i.e. Grant county, and Grant Community Park)
# the \"county\" should get the former, and ignore the latter.

# the fromdos/sed call will drop any dead whitespace at the end of a line

# the test/rm call will delete files if they are zero length.


for foo in `cat $1.counties` ; do
  rm -f $1.$foo
  echo Extracting $foo
  grep ,\"$foo\", $1 | fromdos | sed -e 's/[ ]*$//g' >>$1.$foo.gnis
  test -s $1.$foo.gnis || rm $1.$foo.gnis
done


# clean up after ourselves
rm $1.counties
