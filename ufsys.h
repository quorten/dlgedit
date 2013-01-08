/* User-friendly file system code. */
/* This is platform dependent code: include windows.h before this
   header. */

#ifndef UFSYS_H
#define UFSYS_H

BOOL GetFilenameDialog(char* filename, char* dlgTitle, HWND hwnd,
		       unsigned type);
int AskForSave(HWND hwnd);

extern char curFilename[MAX_PATH];
extern BOOL noFileLoaded;
extern BOOL fileChanged;

#endif /* UFSYS_H */
