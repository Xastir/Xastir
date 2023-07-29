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
6. [Start using Xastir](#start-using-xastir)
7. [Miscellaneous notes](#miscellaneous-notes)

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

## Start using Xastir

Xastir can now be run simply by typing its name at the command line.

The default map (the only one you can view with the bare minimum
install) is ugly and ancient, but if you enabled GraphicsMagick and
curl then there are many online map options available immediately in
the map chooser.  I recommend `Online/OSM_tiled_mapnik.geo`.

You will now have to get Xastir configured and connected to your TNC
or an internet APRS server.

* [Starting Xastir](#starting-xastir)
* [Changing the language](#changing-the-language)
* [Configuring Xastir](#configuring-xastir)
* [Various ways to manipulate Xastir](#various-ways-to-manipulate-xastir)

### Starting Xastir:

NEVER RUN XASTIR AS THE ROOT USER!  You're risking the
security of your system by attempting it.  Create another regular
user on your system and use that user for all of your normal
activity.  This goes for any other normal activity on the system as
well.  Only use the "root" account for maintenance activities, not
for regular user activities.  You'll thank me later!

Assuming you want to start Xastir up in the English language, you
can type (from an xterm window):

    xastir

which will start up the program without giving you back a
command-prompt in your xterm window (until Xastir exits), or you can
type (from an xterm window):

     xastir &

which will start Xastir in the background, giving you back your
xterm for more commands.  The typical way to start it is with
"xastir &".  Of course you can get fancier and attach it to your
window manager's menus or create an icon on your desktop which
starts it.  Those are operating system/window manager-specific, so
we won't cover how to do that here.

The first time you start Xastir it will show a default map of the
world plus pop up the File->Configure->Station dialog.  Enter a
callsign on that dialog and press the OK button.


### Changing the Language:

If you want to start Xastir using some other language, you do that
with command-line switches when you start Xastir.  Once you use one
of these switches, that language option becomes "sticky", meaning
you won't have to enter that command-line switch again unless you
wish to change languages.

There are some command-line switches that you can 
If you type "xastir -?", which is an invalid command-line option,
you'll see this:

```
  xastir: invalid option -- h

  Xastir Command line Options
  -c /path/dir       Xastir config dir
  -f callsign        Track callsign
  -i                 Install private Colormap
  -geometry WxH+X+Y  Set Window Geometry
  -l Dutch           Set the language to Dutch
  -l English         Set the language to English
  -l French          Set the language to French
  -l German          Set the language to German
  -l Italian         Set the language to Italian
  -l Portuguese      Set the language to Portuguese
  -l Spanish         Set the language to Spanish
  -l ElmerFudd       Set the language to ElmerFudd
  -l MuppetsChef     Set the language to MuppetsChef
  -l OldeEnglish     Set the language to OldeEnglish
  -l PigLatin        Set the language to PigLatin
  -l PirateEnglish   Set the language to PirateEnglish
  -m                 Deselect Maps
  -p                 Disable popups
  -t                 Internal SIGSEGV handler enabled
  -v level           Set the debug level
  -V                 Print version number and exit
```

Ignore those for now unless you need to change the Language.

OK, Xastir should show up on your screen at this point.  We're
assuming that you're already running X-Windows graphical environment
at this point.  If you're in command-line Linux/Unix only, Xastir
won't run.

If you've configured in ShapeLib capability, you'll need to run
/usr/local/share/xastir/scripts/get-NWSdata as the root user in order
to get the NOAA data files you'll need for the weather alerts.  The
script requires "wget" in order to work.  Run this script periodically
(once every six months perhaps?) to keep your weather alert maps
up-to-date.  If you're not in the U.S. or one of it's possessions then
you can safely ignore this download.

### Configuring Xastir:

* Note that the menu's have a dashed line near the top.  If you
click on that dashed line it acts like a cut-line for the menu and
detaches that menu from the main menu.  You can then move that menu
off to another area of your screen.  You might try that with the
File->Configure menu at this time.

* Go to File->Configure->Station and set your callsign.  Set up
other parameters/comment fields on this dialog that may need
setting.

* Go to File->Configure->Defaults and set parameters there.

You have the main parameters set now.  Next is to enable some
interfaces so that you can see some packets come across.  Easiest
might be the Internet interfaces, assuming the computer you're on
has Internet access and is hooked up to it currently.

* Run "callpass" in another Xterm window in order to generate your
Pass-code number.  Save that number as you'll need it for each
Interface dialog where you might need to authenticate your callsign.
Of course you can always run callpass again if you forget it!

* Go to Interface->Properties then click on "Add".  Click "Internet
Server".  Another dialog will come up that allows you to enter the
Host, and the Port.  Enter your Pass-code number here.  People often
check the "Activate on Startup?" and the "Reconnect on NET failure?"
options on this box.  You may also assign a comment to this
interface which describes the interface better for you.  Click "OK"
to create the interface.  If you checked "Activate on Startup?" then
the interface will start as well and you'll be receiving packets.

Browse "http://www.aprs2.net/" to find a reasonable set of servers
to start with.  Another possibility is to use "rotate.aprs2.net"
port 14580, which theoretically should rotate among the available
second-tier servers.  See "http://www.aprs2.net" for more info.
You'll need to put in a filter string, such as "r/35/-106/500" which
shows you stations that are within 500km of 35dN/106dW (Thanks for
that one Tom!).  For additional filter settings check out:

    http://www.aprs-is.net/javaprssrvr/javaprsfilter.htm

* Start that interface from the Interface->Start/Stop dialog if
it's not started already.  You'll see icons in the lower right
toggling and see callsigns in the lower left status box if packets
are coming in.

One thing about configuration:  Most things don't get written to
Xastir's config file until you choose either "File->Configure->Save
Config Now!" or you exit Xastir.  Map Selections however are
immediate.

* Creating/starting interfaces for other types of devices is
similar.  If you're wanting to create AX.25 kernel networking ports
you'll have to refer to the HAM HOWTO documents and perhaps the
linux-hams mailing list for help.  For AGWPE connections refer to
that AGWPE docs and mailing list.

It's recommended that if you run a local TNC, you run it in KISS
mode.  You can do that via the Serial KISS TNC interface, or via
AX.25 Kernel Networking ports.

Some of the more esoteric types of interfaces may require some
questions on the Xastir list.  Don't be afraid to ask them as we've
all been there before.

### Various ways to manipulate Xastir


#### Context-Dependent Operations:

The top row of this table refers to the mode of operation.  The
"Cursor" row describes what the cursor looks like when in that mode.
Each following row describes what the operation on the left hand
column performs.

|       | Normal  | Draw-Cad  |  Measure    | Move  |
|:-:|:-:|:-:|:-:|:-:|
| Cursor| Arrow   | Pencil    | Crosshairs  | Crosshairs  |
| LeftClick  |   |   |   | SelectObject  |
| LeftDrag  | ZoomToArea  | ZoomToArea  | MeasureArea  | MoveObject  |
| MiddleClick  | ZoomOut  | SetCADPoint  | ZoomOut  | ZoomOut  |

Alt-F, Alt-V, etc to bring up main menus via the keyboard.  Use
arrow keys to navigate menus and/or single letters corresponding to
the "hot" letter (underlined lettter) for each menu item.

"ESC" to back out of the menu system.


#### Global Operations:

| Action  | Function |
|:--|:--|
|LeftClick|      Select Menu or GUI Item (when in menus or dialogs)|
|LeftDblClick|    FetchAlertText (when in View->Wx Alerts dialog)|
|RightClick |     OptionsMenu|
|Home|            Center the map on your home station|
|PageUp|          ZoomOut|
|PageDown|        ZoomIn|
|ArrowUp|         PanUp|
|ArrowDown|       PanDown|
|ArrowLeft|       PanLeft|
|ArrowRight|      PanRight|
|"="|             GridSize++|
|"+"|             GridSize++|
|"Num+"|          GridSize++|
|"-"|             GridSize--|
|"Num-"|          GridSize--|
|"Space"|         Activate current widget|
|"Tab"|           Rotate among widgets|
|"Back-Tab"|      Rotate among widgets backwards|


#### Other Possible External Stimuli:

If you send Xastir a signal (using "kill"), you can force it to
perform some action based on which signal you send.

* Send a SIGUSR1 to cause a snapshot to be taken.
* Send a SIGHUP to cause Xastir to save/quit/restart.
* Send a SIGINT, SIGQUIT, or SIGTERM to cause Xastir to quit.
* Connect to TCP port 2023 if Server Port is enabled to send/receive packets.
* Send to UDP port 2023 via the `xastir_udp_client` program to inject packets.

## Miscellaneous notes

### A Note About the Map Directory:

The map directory (/usr/local/share/xastir/maps/) is free-form,
meaning you can have links in there, subdirectories, etc.  Organize
it in any way that makes sense to you.  From within the Map Chooser
you can select a directory name, which will select every map
underneath that directory, so keep that in mind while organizing
your maps.


### Enabling Weather Alerts:

You must have Shapelib compiled into Xastir.  Xastir now comes with
Shapelib support built-in.  PRCE/dbfawk are optional.  Install NOAA
shapefile maps as specified in README.MAPS.  These files must be
installed into the /usr/local/share/xastir/Counties/ directory.  You
may use this script to download/install them for you:

    "/usr/local/share/xastir/scripts/get-NWSdata"

which must be run as the root user, and requires "wget" to work.

A neat trick:  You can copy some of these maps into the
/usr/local/share/xastir/maps directory somewhere (a new subdirectory
under there is always fine), then you can select some of these maps
as regular Xastir maps as well.


### Enabling FCC/RAC Callsign Lookup:

Run the /usr/local/share/xastir/scripts/get-fcc-rac.pl script as root,
which will download and install the proper databases into the
/usr/local/share/xastir/fcc/ directory.  At that point the callsign
lookup features in the Station Info dialog and in the "Station->Find
Station" menu option should be functional.


### Enabling Audio Alarms:

Download and install sample audio files from Xastir's GitHub
download site:

    git clone http://github.com/Xastir/xastir-sounds

Copy the files to your Xastir sounds directory, for instance `/usr/local/share/xastir/sounds/`

Install a command-line audio player.  Call out the path/name of that
player in the File->Configure->Audio Alarms dialog.  Common ones are
vplay and auplay, but there are many others.  Enable the types of
alarms you desire in that same dialog.

You should be able to test it manually from a shell by typing the
command in something like this:  vplay filename

Once you find a command that works, type it into Xastir's Audio
Alarms dialog exactly the same except omit the filename.


### Enabling Synthesized Speech:

This is currently available only on  Linux/FreeBSD.

* Install the Festival Speech Synthesizer.  Configure/compile support
for it into Xastir.  Start up the Festival server before starting
Xastir using `festival --server &`.  Xastir should start up and
connect to the server.  Use the options in File->Configure->Speech to
decide which things you'd like Xastir to speak to you about.

Note that the Proximity Alert option in the File->Configure->Speech
dialog uses the distances set in the File->Configure->Audio Alarms
dialog.


### Enabling GPS Waypoint/Track/Route Download Support:

Install GPSMan and gpsmanshp.  Configure/compile support for it in
Xastir.  Start up GPSMan separately and configure it for your GPS
and serial port.  You'll see download options for each type on the
Interface menu.

Note that Xastir requires a version of gpsman at least as recent as 6.1.  Older
versions of gpsman may not work.


### Transmit Enable/Disable Options:

Each interface has a separate transmit enable.  The Interface menu
also has a few global transmit enables.  All of these must be
enabled for a particular interface to transmit.  Also, for Internet
servers, you typically need to authenticate with the server using
your callsign/pass-code before you're allowed to inject packets into
the Internet stream which may get gated out to RF.  If you just want
to talk to other Internet users, you don't need a pass-code to
authenticate to the servers.


### Igating Options:

There are igating options on each local TNC interface.  There are
other global igating options on the File->Configure->Defaults
dialog.  The global option sets restrictions on all igating.


### Where stuff is kept:

Per-user configurations are kept in each user's ~/.xastir directory, by
default.  In particular the ~/.xastir/config/xastir.cnf file is where most
of the configs are kept.  This directory can be optionally specified using
the -c /path/dir command line option.  Make sure you specify a directory,
not a file!  Xastir will create the directory and several subdirectories if
they do not exist when you start up. 

A few executables are installed in /usr/local/bin/.

Scripts are installed in /usr/local/share/xastir/scripts.

Maps are installed in /usr/local/share/xastir/maps/.  Lots of other
directories are under /usr/local/share/xastir/.
