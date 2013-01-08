/* Graphical dialog code, intended to assist hit-testing and rendering. */
/* This is platform dependent code: include windows.h before this
   header. */

#ifndef GRAPHHIT_H
#define GRAPHHIT_H

#define DLG2SCR_X(var) MulDiv(var, dlgBaseX, 4);
#define DLG2SCR_Y(var) MulDiv(var, dlgBaseY, 8);

void ProcLBtnDown(HWND hwnd, LPARAM lParam);
void WindowCursorClip(HWND hwnd);
void ProcMouseMove(HWND hwnd, LPARAM lParam);
void DragHandleTest(HWND hwnd);
void EndDlgClick(HWND hwnd);
void CancelDrag();
void UpdateFont(HDC hDC);

void DrawSelRect(HDC hDC, RECT* rt);
void InvalSelItem(HWND hwnd);
void DrawDlgItemDispatch(HDC hDC, unsigned ctrlNum);
void DrawCRControl(HDC hDC, RECT* rt, char* caption, UINT state);
void DrawButton(HDC hDC, RECT* rt, char* caption, BOOL defBtn);
void DrawClientBox(HDC hDC, RECT* rt, BOOL icon);
void DrawDlgText(HDC hDC, RECT* rt, char* caption, UINT align);
void DrawGroupBox(HDC hDC, RECT* rt, char* caption, UINT textAlign);
void DrawScrollbar(HDC hDC, RECT* rt, UINT type);
void DrawCustomCtrl(HDC hDC, RECT* rt, char* clsName);

/* This is so that we do not need to create a pseudo dialog box for
   MapDialogRect() */
void DlgUnitMap(RECT* rt);

extern HFONT dlgFont;
extern long fontHeight;
extern long avgCharWidth;
extern long dlgBaseX, dlgBaseY;

extern BOOL clickHit;
extern POINT lastDlgPos;
extern int activeCtrl;
extern RECT dragHandles[8];
extern int curDragHand;

#endif /* GRAPHHIT_H */
