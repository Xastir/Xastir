##      -*- autoconf -*-

dnl  This file is free software; you can redistribute it and/or
dnl  modify it under the terms and conditions of the GNU Library
dnl  General Public License as published by the Free Software
dnl  Fondation; either version 2 of the License, or (at your option)
dnl  any later version.
dnl
dnl  This library is distributed in the hope that it will be useful,
dnl  but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
dnl  Library General Public License for more details.
dnl
dnl  You should have received a copy of the GNU General Public License
dnl  along with this library; see the file COPYING.LIB.  If not, write
dnl  to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
dnl  Boston, MA 02111-1307, USA.
dnl
dnl  IMPORTANT NOTE:
dnl  Please do not modify this file unless you expect your changes to be
dnl  carried into every other module in the repository.  If you decide
dnl  that you really want to modify it, contact cbyam@virginia.edu,
dnl  mention that you have and that the modified file should be committed
dnl  to every module.
dnl
dnl  Single module modifications are best placed in configure.in or
dnl  configure.in.in if present.
dnl
dnl  acinclude.m4
dnl     macros autoconf uses when building configure from configure.in
dnl
dnl  $Id$
dnl
dnl    aclocal.m4 -> acinclude.m4
dnl    Make changes to the configure scripts here and not to aclocal.m4
dnl    if you want the them to remain permanent.
dnl


dnl ----------------------------------------------------------------------
dnl XASTIR_MSG_CONFIGURE_START()
dnl
AC_DEFUN(XASTIR_MSG_CONFIGURE_START, [dnl
AC_MSG_RESULT()
AC_MSG_RESULT(=======================================================)
AC_MSG_RESULT(This is Xastir's GNU configure script.)
AC_MSG_RESULT(It's going to run a bunch of strange tests to hopefully)
AC_MSG_RESULT(make your compile work without much twiddling.)
AC_MSG_RESULT(=======================================================)
AC_MSG_RESULT()
])dnl

dnl ----------------------------------------------------------------------
dnl
dnl XASTIR_MSG_CONFIGURE_END()
dnl
AC_DEFUN(XASTIR_MSG_CONFIGURE_END, [dnl
AC_MSG_RESULT()
AC_MSG_RESULT(=====================================================================)
AC_MSG_RESULT(Configure is done.)
AC_MSG_RESULT()
if test -f "./$XASTIREXEC"
then
  AC_MSG_RESULT([Type 'make clean' and then type 'make'])
else
  AC_MSG_RESULT([Type 'make' to create the executable then 'make install' as root.])
  AC_MSG_RESULT([For Solaris, perhaps others: type 'gmake' and then 'gmake install'.])
fi
AC_MSG_RESULT(=====================================================================)
AC_MSG_RESULT()
])dnl


dnl ------------------------------------------------------------------------
dnl Find a file (or one of more files in a list of dirs)
dnl ------------------------------------------------------------------------
dnl
AC_DEFUN(AC_FIND_FILE,
[
$3=NO
for i in $2;
do
  for j in $1;
  do
    if test -r "$i/$j"; then
      $3=$i
      break 2
    fi
  done
done
])


dnl ----------------------------------------------------------------------
dnl
dnl XASTIR_FIND_PATH(program-name, variable-name, list of directories,
dnl       if-not-found, test-parameter)
dnl
AC_DEFUN(XASTIR_FIND_PATH,
[
  AC_MSG_CHECKING([for $1])
  if test -n "$$2"; then
    xastir_cv_path="$$2";
  else
    xastir_cache=`echo $1 | sed 'y%./+-%__p_%'`

    AC_CACHE_VAL(xastir_cv_path$xastir_cache,
    [
    xastir_cv_path="NONE"
    dirs="$3"
    xastir_save_IFS=$IFS
    IFS=':'
    for dir in $PATH; do
      dirs="$dirs $dir"
    done
    IFS=$xastir_save_IFS

    for dir in $dirs; do
      if test -x "$dir/$1"; then
        if test -n "$5"
        then
          evalstr="$dir/$1 $5 2>&1 "
          if eval $evalstr; then
            xastir_cv_path="$dir/$1"
            break
          fi
        else
          xastir_cv_path="$dir/$1"
          break
        fi
      fi
    done

    eval "xastir_cv_path_$xastir_cache=$xastir_cv_path"

    ])
    eval "xastir_cv_path=\"`echo '$xastir_cv_path_'$xastir_cache`\""

  fi

if test
  -z "$xastir_cv_path" || test "xastir_cv_path" = NONE;
then AC_MSG_RESULT(not found)
  $4
else
  AC_MSG_RESULT($xastir_cv_path)
  $2=$xastir_cv_path
fi
])





dnl ----------------------------------------------------------------------
#
# Check to make sure that the build environment is sane.
#
AC_DEFUN(AM_SANITY_CHECK,
[AC_MSG_CHECKING([whether build environment is sane])
# Just in case
sleep 1
echo timestamp > conftestfile
# Do `set' in a subshell so we don't clobber the current shell's
# arguments.  Must try -L first in case configure is actually a
# symlink; some systems play weird games with the mod time of symlinks
# (eg FreeBSD returns the mod time of the symlink's containing
# directory).
if (
   set X `ls -Lt $srcdir/configure conftestfile 2> /dev/null`
   if test "[$]*" = "X"; then
      # -L didn't work.
      set X `ls -t $srcdir/configure conftestfile`
   fi
   if test "[$]*" != "X $srcdir/configure conftestfile" \
      && test "[$]*" != "X conftestfile $srcdir/configure"; then

      # If neither matched, then we have a broken ls.  This can happen
      # if, for instance, CONFIG_SHELL is bash and it inherits a
      # broken ls alias from the environment.  This has actually
      # happened.  Such a system could not be considered "sane".
      AC_MSG_ERROR([ls -t appears to fail.  Make sure there is not a broken
alias in your environment])
   fi

   test "[$]2" = conftestfile
   )
then
   # Ok.
   :
else
   AC_MSG_ERROR([newly created file is older than distributed files!
Check your system clock])
fi
rm -f conftest*
AC_MSG_RESULT(yes)])


dnl AC_VALIDIFY_CXXFLAGS checks for forbidden flags the user may have given
AC_DEFUN(AC_VALIDIFY_CXXFLAGS,
[dnl
 AC_REMOVE_FORBIDDEN(CXX, [-fno-rtti])
 AC_REMOVE_FORBIDDEN(CXXFLAGS, [-fno-rtti])
])

dnl use: AC_REMOVE_FORBIDDEN(CC, [-forbid -bad-option whatever])
dnl it's all white-space separated
AC_DEFUN(AC_REMOVE_FORBIDDEN,
[ __val=$$1
  __forbid=" $2 "
  if test -n "$__val"; then
    __new=""
    ac_save_IFS=$IFS
    IFS="       "
    for i in $__val; do
      case "$__forbid" in
        *" $i "*) AC_MSG_WARN([found forbidden $i in $1, removing it]) ;;
        *) # Careful to not add spaces, where there were none, because otherwise
           # libtool gets confused, if we change e.g. CXX
           if test -z "$__new" ; then __new=$i ; else __new="$__new $i" ; fi ;;
      esac
    done
    IFS=$ac_save_IFS
    $1=$__new
  fi
])


AC_DEFUN(XASTIR_PRUNE_DUPLICATES,
[
  changequote(,)dnl
   CFLAGS=`echo  "$CFLAGS" | awk '{for(i=1;i<=NF;++i) {if (arg[$i]++ == 0) s = s " " $i} print s}'`
  LDFLAGS=`echo "$LDFLAGS" | awk '{for(i=1;i<=NF;++i) {if (arg[$i]++ == 0) s = s " " $i} print s}'`
  changequote([,])dnl
])


AC_DEFUN(XASTIR_CHECK_COMPILER_FLAG,
[
AC_REQUIRE([AC_CHECK_COMPILERS])
AC_MSG_CHECKING(whether $CXX supports -$1)
xastir_cache=`echo $1 | sed 'y%.=/+-%___p_%'`
AC_CACHE_VAL(xastir_cv_prog_cxx_$xastir_cache,
[
echo 'int main() { return 0; }' >conftest.cc
eval "xastir_cv_prog_cxx_$xastir_cache=no"
if test -z "`$CXX -$1 -c conftest.cc 2>&1`"; then
  if test -z "`$CXX -$1 -o conftest conftest.o 2>&1`"; then
    eval "xastir_cv_prog_cxx_$xastir_cache=yes"
  fi
fi
rm -f conftest*
])
if eval "test \"`echo '$xastir_cv_prog_cxx_'$xastir_cache`\" = yes"; then
 AC_MSG_RESULT(yes)
 :
 $2
else
 AC_MSG_RESULT(no)
 :
 $3
fi
])


AC_DEFUN(AC_CHECK_COMPILERS,
[
  dnl this is somehow a fat lie, but prevents other macros from double checking
  AC_PROVIDE([AC_PROG_CC])
  AC_PROVIDE([AC_PROG_CPP])
  AC_PROVIDE([AC_PROG_CXX])
  AC_PROVIDE([AC_PROG_CXXCPP])

  AC_ARG_ENABLE(debug,[  --enable-debug          creates debugging code [default=no]],
  [
   if test $enableval = "no"; dnl
     then
       xastir_use_debug_code="no"
       xastir_use_debug_define=yes
     else
       xastir_use_debug_code="yes"
       xastir_use_debug_define=no
   fi
  ], [xastir_use_debug_code="no"
      xastir_use_debug_define=no
    ])

  AC_ARG_ENABLE(strict,[  --enable-strict         compiles with strict compiler options (may not work!)],
   [
    if test $enableval = "no"; then
         xastir_use_strict_options="no"
       else
         xastir_use_strict_options="yes"
    fi
   ], [xastir_use_strict_options="no"])

  AC_ARG_ENABLE(profile,[  --enable-profile        creates profiling infos [default=no]],
     [xastir_use_profiling=$enableval],
     [xastir_use_profiling="no"]
  )

dnl this was AC_PROG_CC. I had to include it manually, since I had to patch it
  AC_MSG_CHECKING(for a C-Compiler)
  dnl if there is one, print out. if not, don't matter
  AC_MSG_RESULT($CC)

  if test -z "$CC"; then AC_CHECK_PROG(CC, gcc, gcc) fi
  if test -z "$CC"; then AC_CHECK_PROG(CC, cc, cc, , , /usr/ucb/cc) fi
  if test -z "$CC"; then AC_CHECK_PROG(CC, xlc, xlc) fi
  test -z "$CC" && AC_MSG_ERROR([no acceptable cc found in \$PATH])

  AC_PROG_CC_WORKS
  AC_PROG_CC_GNU

  if test $ac_cv_prog_gcc = yes; then
    GCC=yes
  else
    GCC=
  fi

  USER_CFLAGS=$CFLAGS
  CFLAGS=

  if test -z "$CFLAGS"; then
    if test "$xastir_use_debug_code" = "yes"; then
      AC_PROG_CC_G
      if test $ac_cv_prog_cc_g = yes; then
        CFLAGS="-g"
        case $host in
        *-*-linux-gnu)
           CFLAGS="$CFLAGS -ansi -W -Wall -pedantic -Wshadow -Wpointer-arith -Wmissing-prototypes -Wwrite-strings -D_XOPEN_SOURCE -D_BSD_SOURCE"
         ;;
        esac
      fi
    else
      if test "$GCC" = "yes"; then
        CFLAGS="-O2"
      else
        CFLAGS=""
      fi
      if test "$xastir_use_debug_define" = "yes"; then
         CFLAGS="$CFLAGS -DNDEBUG"
      fi
    fi

    if test "$xastir_use_profiling" = yes; then
      XASTIR_PROG_CC_PG
      if test "$xastir_cv_prog_cc_pg" = yes; then
        CFLAGS="$CFLAGS -pg"
      fi
    fi

    if test "$GCC" = "yes"; then
     CFLAGS="$CFLAGS"

     if test "$xastir_use_strict_options" = "yes"; then
        CFLAGS="$CFLAGS -W -Wall -ansi -pedantic -Wshadow -Wpointer-arith -Wmissing-prototypes -Wwrite-strings"
     fi
    fi

  fi

  case "$host" in
  *-*-sysv4.2uw*) CFLAGS="$CFLAGS -D_UNIXWARE";;
  esac

  if test -n "$USER_CFLAGS"; then
    CFLAGS="$CFLAGS $USER_CFLAGS"
  fi

  if test -z "$LDFLAGS" && test "$xastir_use_debug_code" = "no" && test "$GCC" = "yes"; then
     LDFLAGS=""
  fi


dnl this is AC_PROG_CPP. I had to include it here, since autoconf checks
dnl dependencies between AC_PROG_CPP and AC_PROG_CC (or is it automake?)

  AC_MSG_CHECKING(how to run the C preprocessor)
  # On Suns, sometimes $CPP names a directory.
  if test -n "$CPP" && test -d "$CPP"; then
    CPP=
  fi
  if test -z "$CPP"; then
  AC_CACHE_VAL(ac_cv_prog_CPP,
  [  # This must be in double quotes, not single quotes, because CPP may get
    # substituted into the Makefile and "${CC-cc}" will confuse make.
    CPP="${CC-cc} -E"
    # On the NeXT, cc -E runs the code through the compiler's parser,
    # not just through cpp.
    dnl Use a header file that comes with gcc, so configuring glibc
    dnl with a fresh cross-compiler works.
    AC_TRY_CPP([#include <assert.h>
    Syntax Error], ,
    CPP="${CC-cc} -E -traditional-cpp"
    AC_TRY_CPP([#include <assert.h>
    Syntax Error], , CPP=/lib/cpp))
    ac_cv_prog_CPP="$CPP"])dnl
    CPP="$ac_cv_prog_CPP"
  else
    ac_cv_prog_CPP="$CPP"
  fi
  AC_MSG_RESULT($CPP)
  AC_SUBST(CPP)dnl


  AC_MSG_CHECKING(for a C++-Compiler)
  dnl if there is one, print out. if not, don't matter
  AC_MSG_RESULT($CXX)

  if test -z "$CXX"; then AC_CHECK_PROG(CXX, g++, g++) fi
  if test -z "$CXX"; then AC_CHECK_PROG(CXX, CC, CC) fi
  if test -z "$CXX"; then AC_CHECK_PROG(CXX, xlC, xlC) fi
  if test -z "$CXX"; then AC_CHECK_PROG(CXX, DCC, DCC) fi
  test -z "$CXX" && AC_MSG_ERROR([no acceptable C++-compiler found in \$PATH])

  AC_PROG_CXX_WORKS
  AC_PROG_CXX_GNU

  if test $ac_cv_prog_gxx = yes; then
    GXX=yes
  fi

  USER_CXXFLAGS=$CXXFLAGS
  CXXFLAGS=""

  if test -z "$CXXFLAGS"; then
    if test "$xastir_use_debug_code" = "yes"; then
      AC_PROG_CXX_G
      if test $ac_cv_prog_cxx_g = yes; then
        CXXFLAGS="-g"
        case $host in  dnl
        *-*-linux-gnu)
           CXXFLAGS="$CXXFLAGS -ansi -D_XOPEN_SOURCE -D_BSD_SOURCE -Wbad-function-cast -Wcast-align -Wundef -Wconversion"
         ;;
        esac
      fi
    else
      if test "$GXX" = "yes"; then
         CXXFLAGS="-O2"
      fi
      if test "$xastir_use_debug_define" = "yes"; then
         CXXFLAGS="$CXXFLAGS -DNDEBUG"
      fi
    fi

    if test "$xastir_use_profiling" = yes; then
      XASTIR_PROG_CXX_PG
      if test "$xastir_cv_prog_cxx_pg" = yes; then
        CXXFLAGS="$CXXFLAGS -pg"
      fi
    fi

    XASTIR_CHECK_COMPILER_FLAG(fexceptions,
    [
      CXXFLAGS="$CXXFLAGS -fexceptions"
    ])

dnl WABA: Nothing wrong with RTTI, keep it on.
dnl    XASTIR_CHECK_COMPILER_FLAG(fno-rtti,
dnl     [
dnl       CXXFLAGS="$CXXFLAGS -fno-rtti"
dnl     ])

    XASTIR_CHECK_COMPILER_FLAG(fno-check-new,
        [
          CXXFLAGS="$CXXFLAGS -fno-check-new"
        ])

    if test "$GXX" = "yes"; then
       CXXFLAGS="$CXXFLAGS"

       if test true || test "$xastir_use_debug_code" = "yes"; then
         CXXFLAGS="$CXXFLAGS -Wall -pedantic -W -Wpointer-arith -Wmissing-prototypes -Wwrite-strings"

         XASTIR_CHECK_COMPILER_FLAG(Wno-long-long,
         [
           CXXFLAGS="$CXXFLAGS -Wno-long-long"
         ])
         XASTIR_CHECK_COMPILER_FLAG(fno-builtin,
         [
           CXXFLAGS="$CXXFLAGS -fno-builtin"
         ])

       fi

       if test "$xastir_use_strict_options" = "yes"; then
        CXXFLAGS="$CXXFLAGS -Wcast-qual -Wbad-function-cast -Wshadow -Wcast-align"
       fi

       if test "$xastir_very_strict" = "yes"; then
         CXXFLAGS="$CXXFLAGS -Wold-style-cast -Wredundant-decls -Wconversion"
       fi
    fi
  fi

    XASTIR_CHECK_COMPILER_FLAG(fexceptions,
        [
          USE_EXCEPTIONS="-fexceptions"
        ],
          USE_EXCEPTIONS=
        )
    AC_SUBST(USE_EXCEPTIONS)

    XASTIR_CHECK_COMPILER_FLAG(frtti,
        [
          USE_RTTI="-frtti"
        ],
          USE_RTTI=
        )
    AC_SUBST(USE_RTTI)

    case "$host" in
      *-*-irix*)  test "$GXX" = yes && CXXFLAGS="$CXXFLAGS -D_LANGUAGE_C_PLUS_PLUS -D__LANGUAGE_C_PLUS_PLUS" ;;
      *-*-sysv4.2uw*) CXXFLAGS="$CXXFLAGS -D_UNIXWARE";;
    esac

    if test -n "$USER_CXXFLAGS"; then
       CXXFLAGS="$CXXFLAGS $USER_CXXFLAGS"
    fi

    AC_VALIDIFY_CXXFLAGS

    AC_MSG_CHECKING(how to run the C++ preprocessor)
    if test -z "$CXXCPP"; then
      AC_CACHE_VAL(ac_cv_prog_CXXCPP,
      [
         AC_LANG_SAVE[]dnl
         AC_LANG_CPLUSPLUS[]dnl
         CXXCPP="${CXX-g++} -E"
         AC_TRY_CPP([#include <stdlib.h>], , CXXCPP=/lib/cpp)
         ac_cv_prog_CXXCPP="$CXXCPP"
         AC_LANG_RESTORE[]dnl
     ])dnl
     CXXCPP="$ac_cv_prog_CXXCPP"
    fi
    AC_MSG_RESULT($CXXCPP)
    AC_SUBST(CXXCPP)dnl

    # the following is to allow programs, that are known to
    # have problems when compiled with -O2
    if test -n "$CXXFLAGS"; then
      xastir_safe_IFS=$IFS
      IFS=" "
      NOOPT_CXXFLAGS=""
      for i in $CXXFLAGS; do
        case $i in
          -O*)
                ;;
          *)
                NOOPT_CXXFLAGS="$NOOPT_CXXFLAGS $i"
                ;;
        esac
      done
      IFS=$xastir_safe_IFS
    fi
    AC_SUBST(NOOPT_CXXFLAGS)

    XASTIR_CXXFLAGS=
    AC_SUBST(XASTIR_CXXFLAGS)
])




dnl ----------------------------------------------------------------------
# Like AC_CONFIG_HEADER, but automatically create stamp file.
AC_DEFUN(AM_CONFIG_HEADER,
[AC_PREREQ([2.12])
AC_CONFIG_HEADER([$1])
dnl When config.status generates a header, we must update the stamp-h file.
dnl This file resides in the same directory as the config header
dnl that is generated.  We must strip everything past the first ":",
dnl and everything past the last "/".
AC_OUTPUT_COMMANDS(changequote(<<,>>)dnl
ifelse(patsubst(<<$1>>, <<[^ ]>>, <<>>), <<>>,
<<test -z "<<$>>CONFIG_HEADERS" || echo timestamp > patsubst(<<$1>>, <<^\([^:]*/\)?.*>>, <<\1>>)stamp-h<<>>dnl>>,
<<am_indx=1
for am_file in <<$1>>; do
  case " <<$>>CONFIG_HEADERS " in
  *" <<$>>am_file "*<<)>>
    echo timestamp > `echo <<$>>am_file | sed -e 's%:.*%%' -e 's%[^/]*$%%'`stamp-h$am_indx
    ;;
  esac
  am_indx=`expr "<<$>>am_indx" + 1`
done<<>>dnl>>)
changequote([,]))])

dnl ----------------------------------------------------------------------
dnl  XASTIR_CHECK_CCPIPE()
dnl
dnl  Checks whether the compiler supports the `-pipe' flag, which
dnl  speeds up the compilation.
AC_DEFUN(XASTIR_CHECK_CCPIPE, [dnl
if test -z "$no_pipe"
then
  if test -n "$GCC"
  then
    AC_CACHE_CHECK(whether the compiler understands -pipe, xastir_cv_var_ccpipe,
    [dnl
      ac_old_cflags=$CFLAGS
      ac_old_CC="$CC"
      CFLAGS="$CFLAGS -pipe"
      AC_TRY_COMPILE(,, xastir_cv_var_ccpipe="yes", xastir_cv_var_ccpipe="no")
      CC="$ac_old_CC"
    ])
    if test "x$xastir_cv_var_ccpipe" = "xno"; then
      CFLAGS="$ac_old_cflags"
    fi
  fi
fi
])dnl


AC_DEFUN([AM_WITH_DMALLOC],
[AC_MSG_CHECKING(whether to dmalloc library)
AC_ARG_WITH(dmalloc,
[  --with-dmalloc[=ARG]    Compile with dmalloc library],
if test "$withval" = "" -o "$withval" = "yes"; then
  ac_cv_dmalloc="/usr/local"
  AC_DEFINE(WITH_DMALLOC, 1, [Define if using dmalloc debugging malloc package])
else
  ac_cv_dmalloc="$withval"
  AC_DEFINE(WITH_DMALLOC, 0, [Define if using dmalloc debugging malloc package])
fi
AC_MSG_RESULT(yes)
CPPFLAGS="$CPPFLAGS -DDEBUG_DMALLOC -DDMALLOC_FUNC_CHECK -I$ac_cv_dmalloc/include"
LDFLAGS="$LDFLAGS -L$ac_cv_dmalloc/lib"
LIBS="$LIBS -ldmalloc"
,AC_MSG_RESULT(no))
])

AC_DEFUN(XASTIR_CHECK_AX25, [
has_axlib="no"
xastir_has_axlib_inc="no"
AC_CHECK_HEADERS(netax25/axlib.h, xastir_has_axlib_inc="yes")
if test "$xastir_has_axlib_inc" = "yes"
then
AC_CHECK_LIB(ax25, ax25_aton, xastir_has_ax25="yes")
if test "$xastir_has_ax25" = "yes"
then
echo "Found libax25"
has_axlib="yes"
LIBS="$LIBS -lax25"
AC_DEFINE(HAVE_AX25, 1, [Define if using libax25])
else
has_axlib = "no"
AC_DEFINE(HAVE_AX25, 0, [Define if using libax25])
fi
fi
])dnl



dnl ----------------------------------------------------------------------
dnl  XASTIR_CHECK_GEOTIFF_LIBS()
dnl
AC_DEFUN(XASTIR_CHECK_GEOTIFF_LIBS, [dnl
use_geotiff=no
if test "$IRIX" = "yes"
then
  AC_MSG_WARN(Skipping library tests because they CONFUSE Irix.)
else
  AC_CHECK_LIB(tiff, TIFFClose, XASTIR_TIFF_LIB="-ltiff")
  AC_CHECK_LIB(geotiff, GTIFNew, XASTIR_GEOTIFF_LIB="-lgeotiff")
  AC_CHECK_LIB(proj, pj_init, XASTIR_PROJ_LIB="-lproj")
  if test "$XASTIR_PROJ_LIB" = "-lproj"
  then
    if test "$XASTIR_TIFF_LIB" = "-ltiff"
    then
      if test "$XASTIR_GEOTIFF_LIB" = "-lgeotiff"
      then
        AC_DEFINE_UNQUOTED(HAVE_GEOTIFF, 1, [Define if you have GeoTiff Package])
        dnl CFLAGS="-DUSE_GEOTIFF ${CFLAGS}"
        LIBS="${XASTIR_GEOTIFF_LIB} ${XASTIR_PROJ_LIB} ${LIBS} ${XASTIR_TIFF_LIB} "
        dnl AC_SUBST(CFLAGS)
        AC_MSG_RESULT(GeoTIFF support found and will be compiled in.)
        use_geotiff=yes
      fi
    fi
  fi
fi
])



dnl ----------------------------------------------------------------------
dnl  XASTIR_CHECK_SHAPELIB_LIBS()
dnl
AC_DEFUN(XASTIR_CHECK_SHAPELIB_LIBS, [dnl
use_shapelib=no
if test "$IRIX" = "yes"
then
  AC_MSG_WARN(Skipping library tests because they CONFUSE Irix.)
else
  AC_CHECK_LIB(shp, DBFOpen, XASTIR_SHAPELIB_LIB="-lshp")
  if test "$XASTIR_SHAPELIB_LIB" = "-lshp"
  then
    dnl CFLAGS="-DUSE_SHAPELIB ${CFLAGS}"
    AC_DEFINE_UNQUOTED(HAVE_SHAPELIB, 1, [Define if you have ShapeLib])
    LIBS="${XASTIR_SHAPELIB_LIB} ${LIBS}"
    dnl AC_SUBST(CFLAGS)
    AC_MSG_RESULT(Shapelib support found and will be compiled in.)
    use_shapelib=yes
  fi
fi
])



dnl ----------------------------------------------------------------------
dnl  XASTIR_CHECK_IMAGEMAGICK_LIBS()
dnl
AC_DEFUN(XASTIR_CHECK_IMAGEMAGICK_LIBS, [dnl
use_imagemagick=no
if test "$IRIX" = "yes"
then
  AC_MSG_WARN(Skipping library tests because they CONFUSE Irix.)
else
  ac_magick_dirs="/usr/bin /usr/X11/bin /usr/X11R6/bin /usr/local/bin"
  ac_magick_inc_dirs="/usr/X11R6/include/magick /usr/X11/include/magick"

  AC_MSG_CHECKING(for Magick-config)
  AC_FIND_FILE(Magick-config, $ac_magick_dirs, ac_magick_dir)
  if test ! "$ac_magick_dir" = "NO"; then
    AC_MSG_RESULT(yes)
    AC_MSG_CHECKING(for ImageMagick - version >= 5.2.0)
    MAGICK_VERSION=`$ac_magick_dir/Magick-config --version`
    changequote(,)dnl
    MAGICK_MAJOR_VERSION=`echo $MAGICK_VERSION | sed -e "s/^\([0-9]*\).[0-9]*.[0-9]*$/\1/"`
    MAGICK_MINOR_VERSION=`echo $MAGICK_VERSION | sed -e 's/^[0-9]*.\([0-9]*\).[0-9]*$/\1/'`
    changequote([,])dnl
    if test "`expr $MAGICK_MAJOR_VERSION \> 5`" = 1 \
    || test "$MAGICK_MAJOR_VERSION" = 5 \
    && test "`expr $MAGICK_MINOR_VERSION \>= 2`" = 1 ; then
      CFLAGS="`$ac_magick_dir/Magick-config --cflags` `$ac_magick_dir/Magick-config --cppflags` ${CFLAGS}"
      dnl CFLAGS="-I/usr/X11R6/include/magick/ ${CFLAGS}"
      LIBS="${LIBS} `$ac_magick_dir/Magick-config --libs`"
      AC_SUBST(CFLAGS)
      AC_MSG_RESULT(yes ($MAGICK_VERSION))
      AC_DEFINE_UNQUOTED(HAVE_IMAGEMAGICK, 1, [Define if you have ImageMagick])
      use_imagemagick=yes
      dnl for dir in  /usr/local/include/magick \
      dnl      /usr/X11/include/magick \
      dnl      /usr/X11R6/include/X11/magick; do
      dnl      if test -e "$dir/magick/api.h"; then
      dnl        ac_magick_inc_dir="$dir/"
      dnl        CFLAGS="-I$ac_magick_inc_dir ${CFLAGS}"
      dnl        break
      dnl      fi
      dnl done
    else
      AC_MSG_RESULT(no ($MAGICK_VERSION))
      AC_MSG_WARN(ImageMagick support will NOT be compiled in.)
      use_imagemagick=no
    fi
  else
    AC_MSG_RESULT(no)
    AC_MSG_WARN(ImageMagick support will NOT be compiled in.)
    use_imagemagick=no
  fi
fi
])



dnl ----------------------------------------------------------------------
dnl  XASTIR_CHECK_FESTIVAL()
dnl
AC_DEFUN(XASTIR_CHECK_FESTIVAL, [dnl
  AC_MSG_CHECKING([for Festival Speech Synthesizer Support])
if test -f "/usr/bin/festival" || test -f "/usr/local/bin/festival"; then
  AC_MSG_RESULT(yes)
  dnl CFLAGS="-DHAVE_FESTIVAL ${CFLAGS}"
  AC_DEFINE_UNQUOTED(HAVE_FESTIVAL, 1, [Define if you have Festival])
  AC_SUBST(CFLAGS)
  use_festival=yes
else
  AC_MSG_RESULT(no)
  use_festival=no
fi
])dnl



dnl ------------------------------------------------------------------------
dnl Find the header files and libraries for X-Windows. Extended the
dnl macro AC_PATH_X
dnl ------------------------------------------------------------------------
dnl
AC_DEFUN(XASTIR_PATH_X,
[
AC_REQUIRE([AC_PROG_CPP])dnl
AC_REQUIRE([XASTIR_MISC_TESTS])dnl
AC_MSG_CHECKING(for X)
# AC_LANG_SAVE
# AC_LANG_C
AC_CACHE_VAL(xastir_cv_have_x,
[# One or both of the vars are not set, and there is no cached value.
if test "{$x_includes+set}" = set || test "$x_includes" = NONE; then
   xastir_x_includes=NO
else
   xastir_x_includes=$x_includes
fi
if test "{$x_libraries+set}" = set || test "$x_libraries" = NONE; then
   xastir_x_libraries=NO
else
   xastir_x_libraries=$x_libraries
fi

# below we use the standard autoconf calls
ac_x_libraries=$xastir_x_libraries
ac_x_includes=$xastir_x_includes

_AC_PATH_X_DIRECT
dnl AC_PATH_X_XMKMF picks /usr/lib as the path for the X libraries.
dnl Unfortunately, if compiling with the N32 ABI, this is not the correct
dnl location. The correct location is /usr/lib32 or an undefined value
dnl (the linker is smart enough to pick the correct default library).
dnl Things work just fine if you use just AC_PATH_X_DIRECT.
case "$host" in
mips-sgi-irix6*)
  ;;
*)
  _AC_PATH_X_XMKMF
  if test -z "$ac_x_includes"; then
    ac_x_includes="."
  fi
  if test -z "$ac_x_libraries"; then
    ac_x_libraries="/usr/lib"
  fi
esac
#from now on we use our own again

# when the user already gave --x-includes, we ignore
# what the standard autoconf macros told us.
if test "$xastir_x_includes" = NO; then
  xastir_x_includes=$ac_x_includes
fi

# for --x-libraries too
if test "$xastir_x_libraries" = NO; then
  xastir_x_libraries=$ac_x_libraries
fi

if test "$xastir_x_includes" = NO; then
  AC_MSG_ERROR([Can't find X includes. Please check your installation and add the correct paths!])
fi

if test "$xastir_x_libraries" = NO; then
  AC_MSG_ERROR([Can't find X libraries. Please check your installation and add the correct paths!])
fi

# Record where we found X for the cache.
xastir_cv_have_x="have_x=yes \
         xastir_x_includes=$xastir_x_includes xastir_x_libraries=$xastir_x_libraries"
])dnl
eval "$xastir_cv_have_x"

if test "$have_x" != yes; then
  AC_MSG_RESULT($have_x)
  no_x=yes
else
  AC_MSG_RESULT([libraries $xastir_x_libraries, headers $xastir_x_includes])
fi

if test -z "$xastir_x_includes" || test "x$xastir_x_includes" = xNONE; then
  X_INCLUDES=""
  x_includes="."; dnl better than nothing :-
 else
  x_includes=$xastir_x_includes
  X_INCLUDES="-I$x_includes"
fi

if test -z "$xastir_x_libraries" || test "x$xastir_x_libraries" = xNONE; then
  X_LDFLAGS=""
  x_libraries="/usr/lib"; dnl better than nothing :-
 else
  x_libraries=$xastir_x_libraries
  X_LDFLAGS="-L$x_libraries"
fi
all_includes="$X_INCLUDES"
all_libraries="$X_LDFLAGS"

AC_SUBST(X_INCLUDES)
AC_SUBST(X_LDFLAGS)
AC_SUBST(x_libraries)
AC_SUBST(x_includes)

# Check for libraries that X11R6 Xt/Xaw programs need.
ac_save_LDFLAGS="$LDFLAGS"
LDFLAGS="$LDFLAGS $X_LDFLAGS"
# SM needs ICE to (dynamically) link under SunOS 4.x (so we have to
# check for ICE first), but we must link in the order -lSM -lICE or
# we get undefined symbols.  So assume we have SM if we have ICE.
# These have to be linked with before -lX11, unlike the other
# libraries we check for below, so use a different variable.
#  --interran@uluru.Stanford.EDU, kb@cs.umb.edu.
AC_CHECK_LIB(ICE, IceConnectionNumber,
  [LIBSM="-llibSM -lICE"], , $X_EXTRA_LIBS)
AC_SUBST(LIBSM)
LDFLAGS="$ac_save_LDFLAGS"

AC_SUBST(X_PRE_LIBS)

LIB_X11='-lX11 $(LIBSOCKET)'
AC_SUBST(LIB_X11)

AC_MSG_CHECKING(for libXext)
AC_CACHE_VAL(xastir_cv_have_libXext,
[
xastir_ldflags_safe="$LDFLAGS"
xastir_libs_safe="$LIBS"

LDFLAGS="$LDFLAGS $X_LDFLAGS $USER_LDFLAGS"
LIBS="-lXext -lX11 $LIBSOCKET"

AC_TRY_LINK([
#include <stdio.h>
],
[
printf("hello Xext\n");
],
xastir_cv_have_libXext=yes,
xastir_cv_have_libXext=no
   )

LDFLAGS=$xastir_ldflags_safe
LIBS=$xastir_libs_safe
 ])

AC_MSG_RESULT($xastir_cv_have_libXext)

if test "$xastir_cv_have_libXext" = "no"; then
  AC_MSG_ERROR([We need a working libXext to proceed. Since configure
can't find it itself, we stop here assuming that make wouldn't find
them either.])
fi

])

# AC_LANG_RESTORE
])


dnl ----------------------------------------------------------------------
dnl
dnl AC_FIND_MOTIF : find OSF/Motif or LessTif, and provide variables
dnl     to easily use them in a Makefile.
dnl
dnl Adapted from a macro by Andreas Zeller.
dnl
dnl The variables provided are :
dnl     link_motif              (e.g. -L/usr/lesstif/lib -lXm)
dnl     include_motif           (e.g. -I/usr/lesstif/lib)
dnl     motif_libraries         (e.g. /usr/lesstif/lib)
dnl     motif_includes          (e.g. /usr/lesstif/include)
dnl
dnl The link_motif and include_motif variables should be fit to put on
dnl your application's link line in your Makefile.
dnl
dnl Oleo CVS Id: motif.m4,v 1.9 1999/04/09 11:46:49 danny
dnl LessTif CVS $Id$
dnl
AC_DEFUN(AC_FIND_MOTIF,
[
AC_REQUIRE([AC_PATH_XTRA])
motif_includes=
motif_libraries=
dnl AC_ARG_WITH(motif,
dnl [  --without-motif         do not use Motif widgets])
dnl Treat --without-motif like
dnl --without-motif-includes --without-motif-libraries.
dnl if test "$with_motif" = "no"
dnl then
dnl   motif_includes=none
dnl   motif_libraries=none
dnl fi

AC_ARG_WITH(motif-includes,
[  --with-motif-includes=DIR    Motif include files are in DIR],
motif_includes="$withval")

AC_ARG_WITH(motif-libraries,
[  --with-motif-libraries=DIR   Motif libraries are in DIR],
motif_libraries="$withval")

AC_MSG_CHECKING(for Motif)

#
#
# Search the include files.
#
if test "$motif_includes" = ""; then
AC_CACHE_VAL(ac_cv_motif_includes,
[
ac_motif_save_LIBS="$LIBS"
ac_motif_save_INCLUDES="$INCLUDES"
ac_motif_save_CPPFLAGS="$CPPFLAGS"
ac_motif_save_LDFLAGS="$LDFLAGS"
#
LIBS="$X_PRE_LIBS -lXm -lXt -lX11 -Xpm $X_EXTRA_LIBS $LIBS"
INCLUDES="$X_CFLAGS $INCLUDES"
CPPFLAGS="$X_CFLAGS $CPPFLAGS"
LDFLAGS="$X_LIBS $LDFLAGS"
#
ac_cv_motif_includes="none"
AC_TRY_COMPILE([#include <Xm/Xm.h>],[int a;],
[
# Xm/Xm.h is in the standard search path.
ac_cv_motif_includes=
],
[
# Xm/Xm.h is not in the standard search path.
# Locate it and put its directory in `motif_includes'
#
# /usr/include/Motif* are used on HP-UX (Motif).
# /usr/include/X11* are used on HP-UX (X and Athena).
# /usr/dt is used on Solaris (Motif).
# /usr/openwin is used on Solaris (X and Athena).
# Other directories are just guesses.
for dir in "$x_includes" "${prefix}/include" /usr/include /usr/local/include \
           /usr/include/Motif2.0 /usr/include/Motif1.2 /usr/include/Motif1.1 \
           /usr/include/X11R6 /usr/include/X11R5 /usr/include/X11R4 \
           /usr/dt/include /usr/openwin/include \
           /usr/dt/*/include /opt/*/include /usr/include/Motif* \
           "${prefix}"/*/include /usr/*/include /usr/local/*/include \
           "${prefix}"/include/* /usr/include/* /usr/local/include/*; do
if test -f "$dir/Xm/Xm.h"; then
ac_cv_motif_includes="$dir"
break
fi
done
])
#
LIBS="$ac_motif_save_LIBS"
INCLUDES="$ac_motif_save_INCLUDES"
CPPFLAGS="$ac_motif_save_CPPFLAGS"
LDFLAGS="$ac_motif_save_LDFLAGS"
])
motif_includes="$ac_cv_motif_includes"
fi
#
#
# Now for the libraries.
#
if test "$motif_libraries" = ""; then
AC_CACHE_VAL(ac_cv_motif_libraries,
[
ac_motif_save_LIBS="$LIBS"
ac_motif_save_INCLUDES="$INCLUDES"
ac_motif_save_CPPFLAGS="$CPPFLAGS"
ac_motif_save_LDFLAGS="$LDFLAGS"
#
LIBS="$X_PRE_LIBS -lXm -lXt -lX11 $X_EXTRA_LIBS $LIBS"
INCLUDES="$X_CFLAGS $INCLUDES"
CPPFLAGS="$X_CFLAGS $CPPFLAGS"
LDFLAGS="$X_LIBS $LDFLAGS"
#
ac_cv_motif_libraries="none"
AC_TRY_LINK([#include <Xm/Xm.h>],[XtToolkitInitialize();],
[
# libXm.a is in the standard search path.
ac_cv_motif_libraries=
],
[
# libXm.a is not in the standard search path.
# Locate it and put its directory in `motif_libraries'
#
# /usr/lib/Motif* are used on HP-UX (Motif).
# /usr/lib/X11* are used on HP-UX (X and Athena).
# /usr/dt is used on Solaris (Motif).
# /usr/lesstif is used on Linux (Lesstif).
# /usr/openwin is used on Solaris (X and Athena).
# Other directories are just guesses.
for dir in "$x_libraries" "${prefix}/lib" /usr/lib /usr/local/lib \
           /usr/lib/Motif2.0 /usr/lib/Motif1.2 /usr/lib/Motif1.1 \
           /usr/lib/X11R6 /usr/lib/X11R5 /usr/lib/X11R4 /usr/lib/X11 \
           /usr/dt/lib /usr/openwin/lib \
           /usr/dt/*/lib /opt/*/lib /usr/lib/Motif* \
           /usr/lesstif*/lib /usr/lib/Lesstif* \
           "${prefix}"/*/lib /usr/*/lib /usr/local/*/lib \
           "${prefix}"/lib/* /usr/lib/* /usr/local/lib/*; do
if test -d "$dir" && test "`ls $dir/libXm.* 2> /dev/null`" != ""; then
ac_cv_motif_libraries="$dir"
break
fi
done
])
#
LIBS="$ac_motif_save_LIBS"
INCLUDES="$ac_motif_save_INCLUDES"
CPPFLAGS="$ac_motif_save_CPPFLAGS"
LDFLAGS="$ac_motif_save_LDFLAGS"
])
#
motif_libraries="$ac_cv_motif_libraries"
fi
#
# Provide an easier way to link
#
if test "$motif_includes" = "none" -o "$motif_libraries" = "none"; then
        with_motif="no"
else
        with_motif="yes"
fi

if test "$with_motif" != "no"; then
        if test "$motif_libraries" = ""; then
                link_motif="-lXm"
                MOTIF_LIBS="-lXm"
        else
                link_motif="-L$motif_libraries -lXm"
                MOTIF_LIBS="-L$motif_libraries -lXm"
        fi
        if test "$motif_includes" != ""; then
                include_motif="-I$motif_includes"
                MOTIF_CFLAGS="-I$motif_includes"
        fi
        AC_DEFINE([HAVE_MOTIF], 1, [Define if Motif is present.])
else
        with_motif="no"
fi
#
AC_SUBST(link_motif)
AC_SUBST(include_motif)
AC_SUBST(MOTIF_CFLAGS)
AC_SUBST(MOTIF_LIBS)
AC_DEFINE([STIPPLE],[True],[Define if STIPPLE is True.])
#
#
#
motif_libraries_result="$motif_libraries"
motif_includes_result="$motif_includes"
test "$motif_libraries_result" = "" && motif_libraries_result="in default path"
test "$motif_includes_result" = "" && motif_includes_result="in default path"
test "$motif_libraries_result" = "none" && motif_libraries_result="(none)"
test "$motif_includes_result" = "none" && motif_includes_result="(none)"
AC_MSG_RESULT(
  [libraries $motif_libraries_result, headers $motif_includes_result])
])dnl



dnl ----------------------------------------------------------------------
dnl
dnl AC_FIND_XPM : find libXpm, and provide variables
dnl     to easily use them in a Makefile.
dnl
dnl Adapted from a macro by Andreas Zeller.
dnl
dnl The variables provided are :
dnl     link_xpm                (e.g. -L/usr/lesstif/lib -lXm)
dnl     include_xpm             (e.g. -I/usr/lesstif/lib)
dnl     xpm_libraries           (e.g. /usr/lesstif/lib)
dnl     xpm_includes            (e.g. /usr/lesstif/include)
dnl
dnl The link_xpm and include_xpm variables should be fit to put on
dnl your application's link line in your Makefile.
dnl
AC_DEFUN(AC_FIND_XPM,
[
AC_REQUIRE([AC_PATH_XTRA])
xpm_includes=
xpm_libraries=
AC_ARG_WITH(xpm,
[  --without-xpm         do not use Xpm])
dnl Treat --without-xpm like
dnl --without-xpm-includes --without-xpm-libraries.
if test "$with_xpm" = "no"
then
    xpm_includes=no
    xpm_libraries=no
fi
AC_ARG_WITH(xpm-includes,
    [  --with-xpm-includes=DIR    Xpm include files are in DIR], xpm_includes="$withval")
AC_ARG_WITH(xpm-libraries,
    [  --with-xpm-libraries=DIR   Xpm libraries are in DIR], xpm_libraries="$withval")
if test "$xpm_includes" = "no" && test "$xpm_libraries" = "no"
then
    with_xpm="no"
fi

AC_MSG_CHECKING([for Xpm])
if test "$with_xpm" != "no"
then
    #
    #
    # Search the include files.
    #
    if test "$xpm_includes" = ""
    then
        AC_CACHE_VAL(ac_cv_xpm_includes,
        [
        ac_xpm_save_CFLAGS="$CFLAGS"
        ac_xpm_save_CPPFLAGS="$CPPFLAGS"
        #
        CFLAGS="$X_CFLAGS $CFLAGS"
        CPPFLAGS="$X_CFLAGS $CPPFLAGS"
        #
        AC_TRY_COMPILE([#include <X11/xpm.h>],[int a;],
        [
        # X11/xpm.h is in the standard search path.
        ac_cv_xpm_includes=
        ],
    [
    # Else clause for first AC_TRY_COMPILE statement
    #
    AC_TRY_COMPILE([#include <Xm/XpmI.h>],[int a;],
    [
    # Xm/XpmI.h is in the Solaris standard search path.
    ac_cv_xpm_includes=
    xpm_libraries="/usr/lib/libXm.so"
    xpm_skip_linking=1
    ],
        [
        # X11/xpm.h or Xm/XpmI.h are not in the standard search path.
        # Locate one of them and put its directory in `xpm_includes'
        #
        # Other directories are just guesses.
        for dir in "$x_includes" "${prefix}/include" /usr/include /usr/local/include \
                   /usr/include/Motif2.0 /usr/include/Motif1.2 /usr/include/Motif1.1 \
                   /usr/include/X11R6 /usr/include/X11R5 /usr/include/X11R4 \
                   /usr/dt/include /usr/openwin/include /usr/X11R6/include \
                   /usr/dt/*/include /opt/*/include /usr/include/Xpm* \
                   "${prefix}"/*/include /usr/*/include /usr/local/*/include \
                   "${prefix}"/include/* /usr/include/* /usr/local/include/*
        do
            if test -f "$dir/X11/xpm.h"
            then
                  ac_cv_xpm_includes="$dir"
        else
          if test -f "$dir/Xm/XpmI.h"
          then
            ac_cv_xpm_includes="$dir"
            xpm_libraries="/usr/lib/libXm.so"
          fi
            fi
        done
        ]) ])
        #
        CFLAGS="$ac_xpm_save_CFLAGS"
        CPPFLAGS="$ac_xpm_save_CPPFLAGS"
        ])
        xpm_includes="$ac_cv_xpm_includes"
    fi

    if test -z "$xpm_includes"
    then
        xpm_includes_result="default path"
        XPM_CFLAGS=""
    else
        if test "$xpm_includes" = "no"
        then
            xpm_includes_result="told not to use them"
            XPM_CFLAGS="-DNO_XPM"
        else
            xpm_includes_result="$xpm_includes"
            XPM_CFLAGS="-I$xpm_includes"
        fi
    fi
    #
    #
    # Now for the libraries.
    #
    if test "$xpm_libraries" = ""
    then
        AC_CACHE_VAL(ac_cv_xpm_libraries,
        [
        ac_xpm_save_LIBS="$LIBS"
        ac_xpm_save_CFLAGS="$CFLAGS"
        ac_xpm_save_CPPFLAGS="$CPPFLAGS"
        #
        LIBS="-lXpm $X_LIBS -lX11 $X_EXTRA_LIBS $LIBS"
        CFLAGS="$XPM_CFLAGS $X_CFLAGS $CFLAGS"
        CPPFLAGS="$XPM_CFLAGS $X_CFLAGS $CPPFLAGS"
        #
        AC_TRY_LINK([#include <X11/xpm.h>],[XpmAttributesSize();],
        [
        # libXpm.a is in the standard search path.
        ac_cv_xpm_libraries=
        ],
    [
    # Else clause of AC_TRY_LINK
    AC_TRY_LINK([#include <Xm/XpmI.h>],[XpmAttributesSize();],
    [
    # libXm.a is in the standard search path for Solaris.
    ac_cv_xpm_libraries=
    ],
        [
        # libXpm.a or libXm.a are not in the standard search path.
        # Locate one of them and put its directory in `xpm_libraries'
        #
        # Other directories are just guesses.
        for dir in "$x_libraries" "${prefix}/lib" /usr/lib /usr/local/lib \
                   /usr/lib/Xlt2.0 /usr/lib/Xlt1.2 /usr/lib/Xlt1.1 \
                   /usr/lib/X11R6 /usr/lib/X11R5 /usr/lib/X11R4 /usr/lib/X11 \
                   /usr/dt/lib /usr/openwin/lib /usr/X11/lib \
                   /usr/dt/*/lib /opt/*/lib /usr/lib/Xpm* \
                   /usr/lesstif*/lib /usr/lib/Lesstif* \
                   "${prefix}"/*/lib /usr/*/lib /usr/local/*/lib \
                   "${prefix}"/lib/* /usr/lib/* /usr/local/lib/*; do
            for ext in "sl" "so" "a"; do
                  if test -d "$dir" && test -f "$dir/libXpm.$ext"
          then
                    ac_cv_xpm_libraries="$dir"
                    break 2
          fi
          if test -d "$dir" && test -f "$dir/libXm.$ext"
          then
            ac_cv_xpm_libraries="$dir"
            break 2
          fi
            done
        done
        ]) ])
        #
        LIBS="$ac_xpm_save_LIBS"
        CFLAGS="$ac_xpm_save_CFLAGS"
        CPPFLAGS="$ac_xpm_save_CPPFLAGS"
        ])
        #
        xpm_libraries="$ac_cv_xpm_libraries"
    fi
    if test -z "$xpm_libraries"
    then
        xpm_libraries_result="default path"
        XPM_LIBS="-lXpm"
    else
        if test "$xpm_libraries" = "no"
        then
            xpm_libraries_result="told not to use it"
            XPM_LIBS=""
        else
        if test "$xpm_skip_linking" = "1"
        then
            xpm_libraries_result="default path"
            XPM_LIBS=""
        else
            xpm_libraries_result="$xpm_libraries"
                XPM_LIBS="-L$xpm_libraries -lXpm"
        fi
        fi
    fi

    if test "$xpm_skip_linking" = "1"
    then
        # JMT - this is only done if we are running Solaris
        AC_DEFINE([HAVE_XPMI], 1, [Define if Xm/XmpI.h is used.])
        AC_DEFINE([HAVE_XPM], 1, [Define if XPM is present.])
    else
#
# Make sure, whatever we found out, we can link.
#
    ac_xpm_save_LIBS="$LIBS"
    ac_xpm_save_CFLAGS="$CFLAGS"
    ac_xpm_save_CPPFLAGS="$CPPFLAGS"
    #
    LIBS="$XPM_LIBS -lXpm $X_LIBS -lX11 $X_EXTRA_LIBS $LIBS"
    CFLAGS="$XPM_CFLAGS $X_CFLAGS $CFLAGS"
    CPPFLAGS="$XPM_CFLAGS $X_CFLAGS $CPPFLAGS"

    AC_TRY_LINK([#include <X11/xpm.h>],[XpmAttributesSize();],
        [
        #
        # link passed
        #
        AC_DEFINE([HAVE_XPM], 1, [Define if XPM is present.])
        ],
        [
        #
        # link failed
        #
        xpm_libraries_result="test link failed"
        xpm_includes_result="test link failed"
        with_xpm="no"
        XPM_CFLAGS="-DNO_XPM"
        XPM_LIBS=""
        ]) dnl AC_TRY_LINK

    LIBS="$ac_xpm_save_LIBS"
    CFLAGS="$ac_xpm_save_CFLAGS $XPM_CFLAGS"
    CPPFLAGS="$ac_xpm_save_CPPFLAGS"
    fi
else
    xpm_libraries_result="told not to use it"
    xpm_includes_result="told not to use them"
    XPM_CFLAGS="-DNO_XPM"
    XPM_LIBS=""
fi
#
# For the case of apple-darwin, we don't want -lXpm
# included due to other libraries also providing the
# same interfaces
#
case "$host" in
   *-*apple-darwin* )
      XPM_LIBS=""
      ;;
   * )
      ;;
esac
AC_MSG_RESULT([libraries $xpm_libraries_result, headers $xpm_includes_result])
AC_SUBST(XPM_CFLAGS)
AC_SUBST(XPM_LIBS)
AC_SUBST(LIBS)
AC_SUBST(CFLAGS)
])dnl AC_DEFN

dnl ----------------------------------------------------------------------
dnl
AC_DEFUN(XASTIR_CHECK_MOTIF,
[
dnl There are cases where motif may be installed on a system, but X may not
dnl be (Default on hppa1.1-hp-hpux10.20) therefore this must all be wrapped
if test "$no_x" = ""; then
 dnl
 dnl Motif (LessTif)
 dnl
 AC_ARG_WITH(motif,
 [  --without-motif               Do not use Motif, even if detected],
 [              case "${withval}" in
                  y | ye | yes )        usemotif=yes ;;
                  n | no )              usemotif=no ;;
                  * )                   usemotif=yes ;;
                esac],
 [              with_motif=yes])

 dnl
 dnl If you ask not to have Xbae then you can't have Motif either.
 dnl And the other way around.
 dnl
 dnl To make sure people know about this, we'll be harsh and abort.
 dnl
 if test "$with_Xbae" = "no" -a "$with_motif" = "yes" ; then
        AC_MSG_ERROR(Cannot build Motif/LessTif interface without Xbae)
 fi
 if test "$with_Xbae" = "yes" -a "$with_motif" = "no" ; then
        AC_MSG_ERROR(Cannot use Xbae without Motif/LessTif)
 fi

 dnl
 dnl Just in case I misunderstood something - make sure nothing slips through.
 dnl
 test "$with_Xbae" = "no" && with_motif="no"
 test "$with_motif" = "no" && with_Xbae="no"

 dnl
 dnl Checks for Motif
 dnl These should not happen if --without-motif has been used.
 dnl
 dnl if test "$with_motif" = "yes"; then
   AC_FIND_MOTIF
   dnl AC_FIND_XBAE
   dnl AC_FIND_XLT
     CFLAGS="$XLT_CFLAGS $MOTIF_CFLAGS $X_CFLAGS $CFLAGS"
     LIBS="$XLT_LIBS $MOTIF_LIBS $X_LIBS $X_PRE_LIBS -lXt -lX11 -lXext $X_EXTRA_LIBS $LIBS"
   dnl AC_CHECK_LIB(Xpm, main)
 dnl fi
fi dnl "$no_x" = "yes"

])dnl

dnl ======================================================================
dnl
AC_DEFUN(XASTIR_MISC_TESTS,
[
   AC_LANG_C
   dnl Checks for libraries.
   AC_CHECK_LIB(compat, main, [LIBCOMPAT="-lcompat"]) dnl for FreeBSD
   AC_SUBST(LIBCOMPAT)
   xastir_have_crypt=
   AC_CHECK_LIB(crypt, crypt, [LIBCRYPT="-lcrypt"; xastir_have_crypt=yes],
      AC_CHECK_LIB(c, crypt, [xastir_have_crypt=yes], [
        AC_MSG_WARN([you have no crypt in either libcrypt or libc.
You should install libcrypt from another source or configure with PAM
support])
        xastir_have_crypt=no
      ]))
   AC_SUBST(LIBCRYPT)
   if test $xastir_have_crypt = yes; then
      AC_DEFINE_UNQUOTED(HAVE_CRYPT, 1, [Defines if your system has the crypt function])
   fi
   AC_CHECK_KSIZE_T
   AC_LANG_C
   AC_CHECK_LIB(dnet, dnet_ntoa, [X_EXTRA_LIBS="$X_EXTRA_LIBS -ldnet"])
   if test $ac_cv_lib_dnet_dnet_ntoa = no; then
      AC_CHECK_LIB(dnet_stub, dnet_ntoa,
        [X_EXTRA_LIBS="$X_EXTRA_LIBS -ldnet_stub"])
   fi
   AC_CHECK_FUNC(inet_ntoa)
   if test $ac_cv_func_inet_ntoa = no; then
     AC_CHECK_LIB(nsl, inet_ntoa, X_EXTRA_LIBS="$X_EXTRA_LIBS -lnsl")
   fi
   AC_CHECK_FUNC(connect)
   if test $ac_cv_func_connect = no; then
      AC_CHECK_LIB(socket, connect, X_EXTRA_LIBS="-lsocket $X_EXTRA_LIBS", ,
        $X_EXTRA_LIBS)
   fi

   AC_CHECK_FUNC(remove)
   if test $ac_cv_func_remove = no; then
      AC_CHECK_LIB(posix, remove, X_EXTRA_LIBS="$X_EXTRA_LIBS -lposix")
   fi

   # BSDI BSD/OS 2.1 needs -lipc for XOpenDisplay.
   AC_CHECK_FUNC(shmat)
   if test $ac_cv_func_shmat = no; then
     AC_CHECK_LIB(ipc, shmat, X_EXTRA_LIBS="$X_EXTRA_LIBS -lipc")
   fi

   LIBSOCKET="$X_EXTRA_LIBS"
   AC_SUBST(LIBSOCKET)
   AC_SUBST(X_EXTRA_LIBS)
   AC_CHECK_LIB(ucb, killpg, [LIBUCB="-lucb"]) dnl for Solaris2.4
   AC_SUBST(LIBUCB)

   case $host in  dnl this *is* LynxOS specific
   *-*-lynxos* )
        AC_MSG_CHECKING([LynxOS header file wrappers])
        [CFLAGS="$CFLAGS -D__NO_INCLUDE_WARN__"]
        AC_MSG_RESULT(disabled)
        AC_CHECK_LIB(bsd, gethostbyname, [LIBSOCKET="-lbsd"]) dnl for LynxOS
         ;;
   *-*-*bsd* )
        AC_MSG_CHECKING([*BSD specific headers])
        [LIBS="$LIBS -lc_r"]
        AC_MSG_RESULT(ok)
        ;;
    esac

   XASTIR_CHECK_TYPES
   XASTIR_CHECK_LIBDL
# AC_CHECK_BOOL
])


dnl ======================================================================
dnl
AC_DEFUN(XASTIR_CHECK_LIBDL,
[
AC_CHECK_LIB(dl, dlopen, [
LIBDL="-ldl"
ac_cv_have_dlfcn=yes
])

AC_CHECK_LIB(dld, shl_unload, [
LIBDL="-ldld"
ac_cv_have_shload=yes
])

AC_SUBST(LIBDL)
])


dnl ======================================================================
dnl Check for the type of the third argument of getsockname
AC_DEFUN(AC_CHECK_KSIZE_T,
[AC_MSG_CHECKING(for the third argument of getsockname)
AC_CACHE_VAL(ac_cv_ksize_t,
# AC_LANG_SAVE
# AC_LANG_CPLUSPLUS
[AC_TRY_COMPILE([
#include <sys/types.h>
#include <sys/socket.h>
],[
socklen_t a=0;
getsockname(0,(struct sockaddr*)0, &a);
],
ac_cv_ksize_t=socklen_t,
ac_cv_ksize_t=)
if test -z "$ac_cv_ksize_t"; then
ac_safe_cxxflags="$CXXFLAGS"
if test "$GCC" = "yes"; then
  CXXFLAGS="-Werror $CXXFLAGS"
fi
AC_TRY_COMPILE([
#include <sys/types.h>
#include <sys/socket.h>
],[
int a=0;
getsockname(0,(struct sockaddr*)0, &a);
],
ac_cv_ksize_t=int,
ac_cv_ksize_t=size_t)
CXXFLAGS="$ac_safe_cxxflags"
fi
# AC_LANG_RESTORE
])

if test -z "$ac_cv_ksize_t"; then
  ac_cv_ksize_t=int
fi

AC_MSG_RESULT($ac_cv_ksize_t)
AC_DEFINE_UNQUOTED(ksize_t, $ac_cv_ksize_t,
      [Define the type of the third argument for getsockname]
)

])


dnl ======================================================================
dnl
AC_DEFUN(XASTIR_CHECK_TYPES,
[  AC_CHECK_SIZEOF(int, 4)dnl
  AC_CHECK_SIZEOF(long, 4)dnl
  AC_CHECK_SIZEOF(char *, 4)dnl
])dnl

dnl ======================================================================
dnl
AC_DEFUN(XASTIR_CHECK_LIBPTHREAD,
[
have_libpthread="no"
AC_CHECK_LIB(pthread, pthread_create, have_libpthread="yes")
if test "$have_libpthread" = "yes"
then
dnl We don't want "-pthread" for CFLAGS w/Solaris.
dnl We also need "-lpthread -lposix4" for LDFLAGS w/Solaris!
dnl CFLAGS="$CFLAGS -pthread"
dnl LDFLAGS="$LDFLAGS -pthread"
case "$host" in
   *-*-freebsd* )
      CFLAGS="$CFLAGS -pthread"
      LDFLAGS="$LDFLAGS -pthread"
      ;;
   *-*-*sunos* )
      ;;
   *-*-*solaris* )
      LDFLAGS="$LDFLAGS -lpthread -lposix4"
      ;;
   *-*-*linux* )
      CFLAGS="$CFLAGS -pthread"
      LDFLAGS="$LDFLAGS -pthread"
      ;;
   *-*apple-darwin* )
      ;;
   * )
      ;;
esac

AC_DEFINE(HAVE_PTHREAD, 1, [Define if using pthreads])
else
AC_DEFINE(HAVE_PTHREAD, 0, [Define if using pthreads])
fi
])


dnl ======================================================================
# AC_CHECK_LIBM - check for math library
AC_DEFUN(AC_CHECK_LIBM,
[AC_REQUIRE([AC_CANONICAL_HOST])dnl
LIBM=
case "$host" in
*-*-beos* | *-*-cygwin*)
  # These system don't have libm
  ;;
*-ncr-sysv4.3*)
  AC_CHECK_LIB(mw, _mwvalidcheckl, LIBM="-lmw")
  AC_CHECK_LIB(m, main, LIBM="$LIBM -lm")
  ;;
*)
  AC_CHECK_LIB(m, main, LIBM="-lm")
  ;;
esac
  LIBS=$LIBM
])

dnl =======================================================================
AC_DEFUN(XASTIR_TYPE_SOCKLEN_T,
[AC_CACHE_CHECK([for socklen_t], ac_cv_type_socklen_t,
[
  AC_TRY_COMPILE(
    [#include <sys/socket.h>],
    [socklen_t len = 42; return len;],
    ac_cv_type_socklen_t=yes,
    ac_cv_type_socklen_t=no)
])
  if test $ac_cv_type_socklen_t != yes; then
    AC_DEFINE([socklen_t], int, [Define to type of socklen_t.])
  fi
])

dnl ======================================================================
AC_DEFUN(XASTIR_STRUCT_TM_GMTOFF,
[AC_CACHE_CHECK([for tm_gmtoff in struct tm], ac_cv_struct_tm_gmtoff,
[
  AC_TRY_COMPILE(
    [#include <time.h>],
    [struct tm t; t.tm_gmtoff = 3600; return t.tm_gmtoff;],
    ac_cv_struct_tm_gmtoff=yes,
    ac_cv_struct_tm_gmtoff=no)
])
  if test $ac_cv_struct_tm_gmtoff != yes; then
    AC_DEFINE([HAVE_TM_GMTOFF], 1, [Define if struct tm has tm_gmtoff.])
  fi
])

