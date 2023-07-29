# Installing Xastir #

This file is meant to replace the original "INSTALL" file that has
been distributed with Xastir for years.


General build process:

0. [Install all system packages required and any optional packages](#install-all-system-packages-required-and-any-optional-packages)
1. [Get the Xastir source code](#get-the-xastir-source-code)
2. [Bootstrap the source code to create the configure script](#bootstrap-the-source-code-to-create-the-configure-script)
3. [Create a build directory and run configure in it with any necessary options](#create-a-build-directory-and-run-configure-in-it-with-any-necessary-options)
4. [Build Xastir](#build-xastir)
5. [Install the code](#install-the-code)
6. Profit

The procedure for building Xastir from source is fairly generic, and
the most difficult part is assuring that you have all dependent
libraries.

## Install all system packages required and any optional packages ##

**The system-specific build instructions at
http://xastir.org/index.php/Installation_Notes often have the exact
command you need to install all required and optional packages on that
system in one fell swoop, so you would do well to look at those notes
before proceeding with the rest of this section.**

### Absolutely mandatory packages ###

The packages you absolutely must have in order to build Xastir are:

* autoconf
* automake
* gcc and all of its development headers
* glibc and its development headers
* openmotif or lesstiff and its development headers
* the X11 window system and X development headers
* make
* git

If you don't have at least these, you won't be able to build Xastir at
all, and if this is all you have you'll get the most limited version
of Xastir possible.

On some operating systems the libraries and headers needed for
compiling against them are in separate packages, and that's why we
list "and its development headers" above.  You should look at the
system-specific build instructions at
http://xastir.org/index.php/Installation_Notes for more detailed
instructions on how exactly one goes about actually installing these
packages if you are unfamiliar.

Some of these packages may already be installed on your system by
default, or may be installed in clusters by a meta package such as
"build-essential" on Ubuntu, which installs the gcc compiler and all the
libraries and headers it needs to function.


### Optional packages ###

Installing these optional packages enables additional features in
Xastir.  You need both the library and development packages for any of
these packages for which both are available.

* gv and ghostscript (Enables map printing)
* libXpm (enables creation of map snapshots for display and printing)
* GraphicsMagick (Enables access to many formats of map images)
* curl  (Enables access to the web to download maps or other data)
* shapelib (Enables display of vector maps in ESRI shapefile format)
* pcre2 (Perl Compatible Regexps, enables control of how shapefile maps are displayed)
* libgeotiff (Enables display of maps in Geotiff format,
  such as older topographic maps provided by the US Geological
  Survey)
* ax25-apps, ax25-doc, ax25-tools, libax25 (enables Linux kernel
  mode AX.25 for sharing/access of KISS TNC devices)
* festival (enables text-to-speech options)

In some cases, there are alternatives that can provide the same
features:

* wget can be used instead of curl
* ImageMagick 6 (NOT ImageMagick 7) can be used instead of
  GraphicsMagick, but GraphicsMagick is preferred
* The older PCRE library (sometimes called PCRE3, even though the
  *current* version is PCRE2) can be used, but is long past its
  end of life.  PCRE2 is preferred.

Some of the packages listed above depend on other packages themselves,
but the norm of modern package management systems is to install all
the dependencies when the main package is installed.

## Get the Xastir source code ##
There are two ways to get Xastir source code:

1. Get one of the source release "tarballs" from Github at
  https://github.com/Xastir/Xastir/releases
  and explode it.  (Replace X.Y.Z with the release number below)
```
  mkdir -p ~/src/XASTIR
  cp Xastir-Release-X.Y.Z.tar.gz ~/src/XASTIR
  cd ~/src/XASTIR
  tar xzvf Xastir-Release-X.Y.Z.tar.gz
```
  This gets you ONLY the source code for one single release of
  Xastir.  The source code will live in the directory
  `~/src/XASTIR/Xastir-Release-X.Y.Z` 

2. An alternative to the above steps is to use git to download the
  Xastir sources:
```
  mkdir -p ~/src/XASTIR
  cd ~/src/XASTIR
  git  clone https://github.com/Xastir/Xastir.git
```
  This will create a clone of the Xastir git repository in an
  "Xastir" subdirectory of the current directory.  The Xastir source
  code will be in `~/src/XASTIR/Xastir`

  All done!  You now have the latest development sources on your computer.
  Not only that, you have a complete copy of the entire project history
  and access to all prior releases.

## Bootstrap the source code to create the configure script ##
Since Release 2.1.8 the Xastir project has not distributed source code
that is ready to build out of the box, even in release tarballs.  It
must always be "bootstrapped" before proceeding.

1. If you are working from a tarball of source code and followed the
   path suggestions above:
    ```
      cd ~/src/XASTIR/Xastir-Release-X.Y.Z
      ./bootstrap.sh
    ```
2. If you grabbed a clone from git and followed the recommendations above:
    ```
      cd ~/src/XASTIR/Xastir
      ./bootstrap.sh
    ```

In either case, you should see the following output:
```
> ./bootstrap.sh 
    5) Removing autom4te.cache directory...
    4) Running aclocal...
    3) Running autoheader...
    2) Running autoconf...
    1) Running automake...
Bootstrap complete.
```

If you don't see "Bootstrap complete" at the end, it didn't work and
the error messages output should guide you to the source of the
problem (usually this only fails when you have not installed all the
necessary autoconf/automake tools).

This bootstrap procedure creates the "configure" script you need in
the next step.

## Create a build directory and run configure in it with any necessary options ##

In order to build Xastir, the configure script you just created must
be run.  Configure tries to work out what you've got installed and
whether it can find all of the pieces it needs.  In many cases on
Linux you can get away with running it with no options at all and
it'll find everything it needs.  In some cases you might need to
provide it with hints about where to find things like libraries and
headers. And there are a number of configure options that allow you to
controll whether to skip some features you don't want, even if you've
got all the pieces installed that would normally enable them.

We strongly recommend doing configuration and building of Xastir in a
"build directory" rather than right in the same directory as the
source code.  The reason for this is that doing so leaves the source
code directory completely pristine, and only creates new stuff in the
build directory.  This makes it much easier to clean up and start over
(you can simply delete the build directory), and also makes it easier
to spot when something has accidentally been changed in the source
tree.

The examples below all have the same basic steps:  create a
new directory and move into it, then run configure in it.  The only
difference between them is whether you have to give any arguments to
configure.

### The simplest example possible ###

Create a new directory in which to build, and run configure in it:
```
mkdir ~/MyXastirBuildDirectory
cd ~/MyXastirBuildDirectory
~/src/XASTIR/Xastir/configure
```

Here we're assuming you got Xastir source code from git and ran
bootstrap.sh, and bootstrap.sh completed without error.

This may be all you need, and if all you have done was install the
minimum required libraries you should see configure exit with this
message:
```
xastir X.Y.Z has been configured to use the following
options and external libraries:

MINIMUM OPTIONS:
  ShapeLib (Vector maps) .................... : no

RECOMMENDED OPTIONS:
  Xpm / Snapshots ........................... : no
  GraphicsMagick/ImageMagick (Raster maps) .. : no
  pcre (Shapefile customization) ............ : no
  Berkeley DB map caching-Raster map speedups : no
  internet map retrieval .................... : no

FOR THE ADVENTUROUS:
  AX25 (Linux Kernel I/O Drivers) ........... : no
  libproj (USGS Topos & Aerial Photos) ...... : no
  GeoTiff (USGS Topos & Aerial Photos) ...... : no
  Festival (Text-to-speech) ................. : no
  GPSMan/gpsmanshp (GPS downloads) .......... : no

xastir will be installed in /usr/local/bin.
Type 'make' to build Xastir (Use 'gmake' instead on some systems).
```

If you've installed optional libraries and they were properly located
by configure, then some of these "no" lines will have "yes" instead.

### Slightly more involved examples ###

Some systems like to install the headers for "libgeotif" into
"/usr/include/geotiff" instead of directly into "/usr/include".  If
that is the case, configure simply won't find them.  In that case, one
has to tell configure to tell the C preprocessor to add an additional
place to look for header files.  So on those systems one would have to
do:

```
cd ~/MyXastirBuildDirectory
~/src/XASTIR/Xastir/configure CPPFLAGS="-I/usr/include/geotiff"
```

On most Linux systems, that's the extent of the complexity you might
need.

### When lots of stuff is in weird places ###

Similarly, on systems like FreeBSD, third-party packages do not
install *anything* into "/usr/include" or "/usr/lib" but rather into
"/usr/local/lib" or "/usr/local/include".  In that case, you'd have to
tell the C preprocessor and linker to look in extra places, too:
```
cd ~/MyXastirBuildDirectory
~/src/XASTIR/Xastir/configure CPPFLAGS="-I/usr/local/include" LDFLAGS="-L/usr/local/lib"
```

Note that systems like Macs often require add-on package managers like
"homebrew" or "macports" which love to put headers and libraries in
strange places where the C compiler and linker don't look by default.
The techniques above apply there, too.  You can stack up options, too,
such as:
```
cd ~/MyXastirBuildDirectory
~/src/XASTIR/Xastir/configure CPPFLAGS="-I/usr/local/include -I/some/weird/place/include" LDFLAGS="-L/usr/local/lib -L/some/weird/place/lib"
```

### A much more involved example ###

On my own system, the Berkeley DB libraries are in a weird place, and
GraphicsMagick will not work with the default system C compiler at
all.  So in my case, I have to run configure as:
```
cd ~/MyXastirBuildDirectory
~/src/XASTIR/Xastir/configure --with-bdb-incdir=/usr/local/include/db5 --with-bdb-libdir=/usr/local/lib CFLAGS="-O2 -g" CC=gcc10 CXX=g++10
```

Here, I'm telling it to look in special places when looking for
Berkeley DB headers and libraries, to use a specific (non-default)
compiler, and to use specific compiler options (CFLAGS).


## Build Xastir ##

Once configure has completed, has announced that it has really
succeeded and that you can "make" the code, and all the options it
shows as enabled are in fact the options you wanted enabled, then just
build it (I'm assuming you're still in your build directory):

```
make
```

You should see a bunch of status updates about what it's doing and
what file is being compiled, and when it's all done you should see no
statement from "make" that it has aborted due to errors (you may see a
handful of warnings, but you can safely ignore those).

Once it's all done you can make a quick test to make sure a valid
executable was created by typing:
```
./src/xastir -V
```
which should respond with a version number, such as:
```
Xastir V2.1.9 (Release-2.1.8-47-g00ce2b88)
```

This particular version number indicates that we're building from a
git clone, that it's Version 2.1.9, and that 47 code commits have
happened since the prior release 2.1.8, and that the current git
commit reference is 00ce2b88.  You can ignore all that if you don't
know what it means.  The important thing that it produced a version
number and not an error message.

That's it, you've built Xastir with the options that configure said
you enabled.

## Install the code ##

You've built Xastir, but it still lives in your home directory
somewhere.  If you want it to be generally accessible on your system,
you need to install it.  You must be root to do this.

```
cd ~/MyXastirBuildDirectory
sudo make install
```

Xastir will by default have installed itself into /usr/local/bin by
this process (and its support files will generally have been installed
into /usr/local/share/xastir).

If you're running on a Linux system and have enabled AX.25 networking,
you also need to do the following command to give the binary access to
kernel AX.25 ports:
```
sudo chmod 4755 /usr/local/bin/xastir
```

## Profit ##

Xastir can now be run simply by typing its name at the command line.

The default map (the only one you can view with the bare minimum
install) is ugly and ancient, but if you enabled GraphicsMagick and
curl then there are many online map options available immediately in
the map chooser.  I recommend `Online/OSM_tiled_mapnik.geo`.

You will now have to get Xastir configured and connected to your TNC
or an internet APRS server, but that's a matter for another file.
