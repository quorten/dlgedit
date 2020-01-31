Dialog Editor README
====================

This dialog editor is a simple yet effective program to help you
design dialogs that you need.  It is not meant to be a feature packed
program with tons of direct support for rendering dialogs exactly as
they will appear in the actual program.  It is simply a program that
allows you to position abstract rectangular dialog elements the way
you want your real dialogs to look like.

You should make sure that you've read the relevant sections in the
Windows Platform SDK on dialogs before using this program.  That will
greatly facilitate your programming capabilities.

How to Use
==========

To add a new control to a dialog template you are editing, enter its
dialog template code in the Text Window, then use the command "Add
Control" (Ctrl+E).  To modify the code of a selected control, change
its code in the Text Window, then use the command "Apply Text"
(Ctrl+W).  You don't need a mouse to graphically edit the dialog
template.  Use "Tab" and "Shift+Tab" to select a control, and the
arrow keys to move a control.  Hold down the Control key and use the
arrow keys to move a control one dialog unit.

The program has support for parsing and rendering all controls that
are specified with a special statement within the dialog template.
These controls are AUTO3STATE, STATE3, AUTOCHECKBOX, CHECKBOX,
AUTORADIOBUTTON, RADIOBUTTON, EDITTEXT, LISTBOX, COMBOBOX, ICON,
LTEXT, CTEXT, RTEXT, PUSHBOX, DEFPUSHBUTTON, PUSHBUTTON, GROUPBOX, and
SCROLLBAR.  Note that each of these controls only has limited support
for rendering in certain styles.  Other controls specified with
"CONTROL" are only drawn as a gray box with the class name inside the
box.  If you need to preview more types of controls, you can modify
the program's source code to add the necessary parsers and rendering
functions to the program.  The source code is fairly modular and
small, so this should not be a difficult task.

Additional Notes
================

Note to WYSIWYG users: Dialog base units can often times refer to
fractional pixel quantities, which means that unit rounding can cause
dialog elements of equal dialog base unit spacing to have different
on-screen pixel spacings.  However, changing the dialog units of such
controls will cause them to have less accurate spacing when rendered
in higher resolutions, so you are recommended to stick to dialog units
when equal spacing is required.

The dialog template parser code does not skip comments, so you may
experience unusual behavior in template files that contain comments.
The parser code also requires that all parameters to a control
statement are on the same line.

Even though I have wanted to make this program be cross-platform, I
soon realized that such a wish would be just about impossible.  The
reason why is because Windows dialog editing is highly dependent on
the font used for the dialog being edited.  Considering that the
majority of Windows dialogs use the proprietary fonts that come with
Windows, this technically constrains Windows dialog editing to only be
possible on Microsoft Windows.

Notes on dialog font selection: On Windows 2000/XP, the system dialog
font can vary between different versions.  On earlier Windows
operating systems, the system dialog font is MS Sans Serif, but on
Windows 2000/XP, the system font may be Tahoma.  To design dialogs
that always select the system dialog font, programmers are encouraged
to set the font family of the dialog to "MS Shell Dlg", set the
DS_SHELLFONT dialog style, and use the DIALOGEX resource instead of
DIALOG.

However, when this Dialog Editor loads a dialog template, it just uses
a call to CreateFont() to get the font handle, which will always
return MS Sans Serif when specified MS Shell Dlg, independent of the
operating system version.  To make sure that you are designing your
dialogs properly, you should manually set the font family to Tahoma
when you are editing such dialogs for Windows XP.

Copy Conditions
===============

The main source code is all in the Public Domain.

See the file "UNLICENSE" in the top level directory for details.

The Windows configuration script `configure.bat` came from GNU Emacs
22.1 and is covered by the GNU General Public License, version 2 or
later.  See the file `COPYING-CONF` for details.
