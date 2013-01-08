/* Header for main source file.  Only declares functions that should
   have public definitions. */
/* This is platform dependent code: include windows.h before this
   header. */

#ifndef DLGEDIT_H
#define DLGEDIT_H

BOOL LoadDialogTemplate(char* filename, BOOL useNewTmpl);
BOOL SaveDialogTemplate(char* filename);

void SetDlgHFont(HDC hDC);
void UpdateTextWindow();
void KbdMoveProc(HWND hwnd, WPARAM wParam);

extern HINSTANCE g_hInstance;

#endif /* DLGEDIT_H */
