Building the Dialog Editor
==========================

This program is quite obviously intended for programmers, so if you do
not yet have a C/C++ compiler environment set up, that should be the
first thing you do.  After all, Windows software development is
primarily targeted at a C/C++ compiler environment.

You can build this program with Microsoft Visual C++, MinGW, or
Cygwin, but building with Cygwin is not recommended.  Use
`Makefile.mingw` to compile with MinGW, `Makefile.msvc` to compile
with MSVC.  Cross compiling is supported in the MinGW makefile, set
the `CROSS` variable according to the toolchain prefix when calling
`make`.

Since you will probably want to modify the source code to add
enhancements, you probably want to build a debug binary (the default).

Note that there are some compiler flags in MSVC8 that this project
uses, and that you may get warnings about unrecognized options if you
use an earlier version.

* Compiler options not in MSVC6: `-Wp64 -GL -RTC1`
* Linker options not in MSVC6: `-MANIFEST -MANIFESTFILE: -LTCG`

If you are using an older version of the Platform SDK on MSVC6, then
you might need to modify "about.dlg" by replacing `DIALOGEX` with `DIALOG`
and removing `DS_SHELLFONT`.

MinGW GCC 3.x is recommended for the most compact code generation.
Newer versions will still work, but they'll copy more unused
boilerplate `libc` and `libgcc` initialization code in by default.

Running the proper make command will produce the binary.  This binary
does not write registry entries, so you do not have to worry about the
program cluttering your system.  Of course, the conscientious user
will know that even though the program doesn't write registry entries
explicitly, the common dialog component of the Windows operating
system still does write registry entries for the most recently used
directory.
