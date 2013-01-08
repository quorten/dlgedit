@echo off
rem   ----------------------------------------------------------------------
rem   Configuration script for MS Windows 95/98/Me and NT/2000/XP
rem   Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005,
rem      2006, 2007 Free Software Foundation, Inc.

rem   This file originated from GNU Emacs, with modifications by Andrew
rem   Makousky to suit the purpose of the Dialog Editor build system.

rem   GNU Emacs is free software; you can redistribute it and/or modify
rem   it under the terms of the GNU General Public License as published by
rem   the Free Software Foundation; either version 2, or (at your option)
rem   any later version.

rem   GNU Emacs is distributed in the hope that it will be useful,
rem   but WITHOUT ANY WARRANTY; without even the implied warranty of
rem   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
rem   GNU General Public License for more details.

rem   You should have received a copy of the GNU General Public License
rem   along with the Dialog Editor; see the file COPYING-CONF.  If not,
rem   write to the Free Software Foundation, Inc., 51 Franklin Street,
rem   Fifth Floor, Boston, MA 02110-1301, USA.
rem   ----------------------------------------------------------------------
rem   Change Log:

rem   2011-08-17  Andrew Makousky
rem   Code was added for detecting whether MSVC supports manifests.

rem   2011-08-16  Andrew Makousky
rem   The irrelevent image library detection code was removed and the
rem   makefile generation code was modified as necessary.  References to
rem   GNU Emacs were switched to the Dialog Editor as appropriate.
rem   ----------------------------------------------------------------------

if exist config.log del config.log

rem ----------------------------------------------------------------------
rem   See if the environment is large enough.  We need 43 (?) bytes.
set $foo$=123456789_123456789_123456789_123456789_123
if not "%$foo$%" == "123456789_123456789_123456789_123456789_123" goto SmallEnv
set $foo$=

rem ----------------------------------------------------------------------
rem   Make sure we are running in the source code directory
if exist configure.bat goto start
echo You must run configure from the same directory as the source code.
goto end

:start
rem ----------------------------------------------------------------------
rem   Default settings.
set nodebug=N
set nocygwin=N
set COMPILER=
set usercflags=
set userldflags=
set have_mt=
set sep1=
set sep2=

rem ----------------------------------------------------------------------
rem   Handle arguments.
:again
if "%1" == "-h" goto usage
if "%1" == "--help" goto usage
if "%1" == "--with-gcc" goto withgcc
if "%1" == "--with-msvc" goto withmsvc
if "%1" == "--no-debug" goto nodebug
if "%1" == "--no-cygwin" goto nocygwin
if "%1" == "--cflags" goto usercflags
if "%1" == "--ldflags" goto userldflags
if "%1" == "" goto checkcompiler
:usage
echo Usage: configure [options]
echo Options:
echo.   --with-gcc              use GCC to compile the Dialog Editor
echo.   --with-msvc             use MSVC to compile the Dialog Editor
echo.   --no-debug              build a release executable
echo.   --no-cygwin             use -mno-cygwin option with GCC
echo.   --cflags FLAG           pass FLAG to compiler
echo.   --ldflags FLAG          pass FLAG to compiler when linking
goto end
rem ----------------------------------------------------------------------
:withgcc
set COMPILER=gcc
shift
goto again
rem ----------------------------------------------------------------------
:withmsvc
set COMPILER=cl
shift
goto again
rem ----------------------------------------------------------------------
:nodebug
set nodebug=Y
shift
goto again
rem ----------------------------------------------------------------------
:nocygwin
set nocygwin=Y
shift
goto again
rem ----------------------------------------------------------------------
:usercflags
shift
set usercflags=%usercflags%%sep1%%1
set sep1= %nothing%
shift
goto again
rem ----------------------------------------------------------------------
:userldflags
shift
set userldflags=%userldflags%%sep2%%1
set sep2= %nothing%
shift
goto again

rem ----------------------------------------------------------------------
rem   Auto-detect compiler if not specified, and validate GCC if chosen.
:checkcompiler
if (%COMPILER%)==(cl) goto compilercheckdone
if (%COMPILER%)==(gcc) goto checkgcc

echo Checking whether 'cl' is available...
echo main(){} >junk.c
cl -nologo -c junk.c
if exist junk.obj goto clOK

echo Checking whether 'gcc' is available...
gcc -c junk.c
if not exist junk.o goto nocompiler
del junk.o

:checkgcc
Rem WARNING -- COMMAND.COM on some systems only looks at the first
Rem            8 characters of a label.  So do NOT be tempted to change
Rem            chkapi* into something fancier like checkw32api
Rem You HAVE been warned!
if (%nocygwin%) == (Y) goto chkapi
echo Checking whether gcc requires '-mno-cygwin'...
echo #include "cygwin/version.h" >junk.c
echo main(){} >>junk.c
echo gcc -c junk.c >>config.log
gcc -c junk.c >>config.log 2>&1
if not exist junk.o goto chkapi
echo gcc -mno-cygwin -c junk.c >>config.log
gcc -mno-cygwin -c junk.c >>config.log 2>&1
if exist junk.o set nocygwin=Y
del junk.c
del junk.o

:chkapi
echo The failed program was: >>config.log
type junk.c >>config.log
rem ----------------------------------------------------------------------
rem   Older versions of the Windows API headers either don't have any of
rem   the IMAGE_xxx definitions (the headers that come with Cygwin b20.1
rem   are like this), or have a typo in the definition of
rem   IMAGE_FIRST_SECTION (the headers with gcc/mingw32 2.95 have this
rem   problem).  The gcc/mingw32 2.95.2 headers are okay, as are distros
rem   of w32api-xxx.zip from Anders Norlander since 1999-11-18 at least.
rem
echo Checking whether W32 API headers are too old...
echo #include "windows.h" >junk.c
echo test(PIMAGE_NT_HEADERS pHeader) >>junk.c
echo {PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(pHeader);} >>junk.c
if (%nocygwin%) == (Y) goto chkapi1
set cf=%usercflags%
goto chkapi2
:chkapi1
set cf=%usercflags% -mno-cygwin
:chkapi2
echo on
gcc %cf% -c junk.c
@echo off
@echo gcc %cf% -c junk.c >>config.log
gcc %cf% -c junk.c >>config.log 2>&1
set cf=
if exist junk.o goto gccOk
echo The failed program was: >>config.log
type junk.c >>config.log

:nocompiler
echo.
echo Configure failed.
echo To configure the Dialog Editor for Windows, you need to
echo have either gcc-2.95 or later with Mingw32 and the W32 API
echo headers, or MSVC 2.x or later.
del junk.c
goto end

:gccOk
set COMPILER=gcc
echo Using 'gcc'
del junk.c
del junk.o
Rem It is not clear what GCC version began supporting -mtune
Rem and pentium4 on x86, so check this explicitly.
echo main(){} >junk.c
echo gcc -c -O2 -mtune=pentium4 junk.c >>config.log
gcc -c -O2 -mtune=pentium4 junk.c >>config.log 2>&1
if not errorlevel 1 goto gccMtuneOk
echo The failed program was: >>config.log
type junk.c >>config.log
set mf=-mcpu=i686
del junk.c
del junk.o
goto compilercheckdone
:gccMtuneOk
echo GCC supports -mtune=pentium4 >>config.log
set mf=-mtune=pentium4
del junk.c
del junk.o
goto compilercheckdone

:clOk
set COMPILER=cl
del junk.c
del junk.obj
echo Using 'MSVC'
echo Checking whether manifest support is available...
echo main(){} >junk.c
cl -nologo junk.c -link -manifest
if exist junk.exe.manifest set have_mt=Y
if (%have_mt%) == (Y) echo Manifest support found.
if (%have_mt%) == () echo No manifest support found.
del junk.exe.manifest
del junk.exe
del junk.obj
del junk.c
rem Perhaps we should check the compiler version and make sure we do not
rem pass unrecognized options, but as long as it does not cause errors,
rem we will not do this.

:compilercheckdone

rem ----------------------------------------------------------------------
:genmakefiles
echo Generating makefiles
if %COMPILER% == gcc set MAKECMD=gmake
if %COMPILER% == cl set MAKECMD=nmake

rem   Pass on chosen settings to makefiles.
rem   NB. Be very careful to not have a space before redirection symbols
rem   except when there is a preceding digit, when a space is required.
rem
echo # Start of settings from configure.bat >config.settings
echo COMPILER=%COMPILER%>>config.settings
if not "(%mf%)" == "()" echo MCPU_FLAG=%mf%>>config.settings
if (%nodebug%) == (Y) echo NODEBUG=1 >>config.settings
if (%nocygwin%) == (Y) echo NOCYGWIN=1 >>config.settings
if not "(%usercflags%)" == "()" echo USER_CFLAGS=%usercflags%>>config.settings
if not "(%userldflags%)" == "()" echo USER_LDFLAGS=%userldflags%>>config.settings
if (%have_mt%) == (Y) echo HAVE_MT=1 >>config.settings
echo # End of settings from configure.bat>>config.settings
echo.>>config.settings

copy /b config.settings+%MAKECMD%.defs+makefile.w32-in makefile
goto end

:SmallEnv
echo Your environment size is too small.  Please enlarge it and rerun configure.
echo For example, type "command.com /e:2048" to have 2048 bytes available.
set $foo$=
:end
set nodebug=
set nocygwin=
set COMPILER=
set MAKECMD=
set usercflags=
set userldflags=
set mingwflag=
set mf=
set have_mt=
