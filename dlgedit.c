/* A simple dialog editor */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>

#include <stdio.h>
#include <string.h>

#include "xmalloc.h"
#include "tmplparser.h"
#include "graphhit.h"
#include "ufsys.h"

#include "resource.h"

/* Microsoft Visual C++ memory leak detection. (This program has NO
   memory leaks, but it is here just to be on the safe side.) */
#ifdef _DEBUG
#include <crtdbg.h>
#endif

/* MSVC >= 8.0 pragmas */
#if _MSC_VER >= 1400
#pragma warning (disable: 4996) /* Disable deprecate */
#pragma warning (disable: 4267) /* Disable size_t warnings */
#endif

/* Windows variables */
HINSTANCE g_hInstance;
HWND textHwnd; /* Container window for text editor */
HWND textBox; /* Actual text edit control */
HFONT teFont = NULL; /* Text edit window font */

LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
								LPARAM lParam);
LRESULT CALLBACK TextWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
								LPARAM lParam);
INT_PTR CALLBACK AboutBoxProc(HWND hDlg, UINT uMsg, WPARAM wParam,
							  LPARAM lParam);

BOOL LoadDialogTemplate(char* filename, BOOL useNewTmpl);
BOOL SaveDialogTemplate(char* filename);

void SetDlgHFont(HDC hDC);
void UpdateTextWindow();
void KbdMoveProc(HWND hwnd, WPARAM wParam);

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInst,
					 LPSTR lpCmdLine, int nShowCmd)
{
	HWND hwnd;			/* Window handle */
	WNDCLASSEX wcex;	/* Window class */
	MSG msg;			/* Message structure */
	HACCEL hAccel;		/* Accelerator table */

	/* Register the main window class */
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = MainWindowProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, (LPCTSTR)MAIN_FRAME);
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName = (LPCTSTR)MAIN_FRAME;
	wcex.lpszClassName = "mainWindow";
	wcex.hIconSm = NULL;

	if (!RegisterClassEx(&wcex))
		return 0;

	/* Create a window class for the text editor window */
	wcex.lpfnWndProc = TextWindowProc;
	wcex.hbrBackground = NULL;
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = "DETextWindow";

	if (!RegisterClassEx(&wcex))
		return 0;

	/* Create a pseudo-window class for drawing the title bar of the dialog */
	wcex.lpfnWndProc = DefWindowProc;
	wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wcex.hbrBackground = NULL;
	wcex.lpszClassName = "DEPseudoWindow";

	if (!RegisterClassEx(&wcex))
		return 0;

	/* Load the accelerator table */
	hAccel = LoadAccelerators(hInstance, (LPCTSTR)MAIN_FRAME);

	/* Globally save the application instance */
	g_hInstance = hInstance;

	/* Create the main window */
	hwnd = CreateWindowEx(0, "mainWindow", "Dialog Editor",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,	/* window position does not matter */
		CW_USEDEFAULT, CW_USEDEFAULT,	/* window size does not matter */
		NULL, 0, hInstance, NULL);

	if (!hwnd)
	{
		MessageBox(NULL, "Error creating main application window.", NULL,
			MB_OK | MB_ICONERROR);
		return 0;
	}

	/* Show the text window on startup */
	textHwnd = CreateWindowEx(0, "DETextWindow", "Text Window",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN,
		CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT,
		NULL, 0, hInstance, NULL);

	if (!textHwnd)
	{
		MessageBox(NULL, "Error creating application text window.", NULL,
			MB_OK | MB_ICONERROR);
		return 0;
	}

	/* Show and draw (update) the window */
	ShowWindow(hwnd, nShowCmd);
	UpdateWindow(hwnd);

	/* Message loop */
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(hwnd, hAccel, &msg))
			TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

#ifdef _DEBUG
	_CrtDumpMemoryLeaks();
#endif
	return (int)msg.wParam;
}

LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
								LPARAM lParam)
{
	static HWND pseudoHwnd; /* Used to draw the dialog title bar */
	static BOOL firstSize = TRUE;
	switch (uMsg)
	{
	case WM_CREATE:
	{
		HDC hDC;
		dlgFont = NULL;

		/* Create a hidden pseudo-window (for drawing the caption) */
		dlgHasCaption = TRUE;
		pseudoHwnd = CreateWindowEx(0, "DEPseudoWindow", "Error",
			WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
			CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, g_hInstance, NULL);

		LoadDialogTemplate(curFilename, TRUE);
		if (dlgHasCaption == TRUE)
			SetWindowText(pseudoHwnd, dlgCaption);
		hDC = GetDC(hwnd);
		SetDlgHFont(hDC);
		UpdateFont(hDC);
		ReleaseDC(hwnd, hDC);
		break;
	}
	case WM_CLOSE:
		if (!AskForSave(hwnd))
			return 0;
		break;
	case WM_DESTROY:
		if (textHwnd != NULL)
			DestroyWindow(textHwnd);
		DestroyWindow(pseudoHwnd);
		DeleteObject(dlgFont);
		FreeDlgData();
		PostQuitMessage(0);
		break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hDC;
		RECT rt;
		HBRUSH hBr;
		hDC = BeginPaint(hwnd, &ps);
		SelectObject(hDC, dlgFont);
		{
			POINT scPos;
			scPos.x = DLG2SCR_X(dlgPos.x);
			scPos.y = DLG2SCR_Y(dlgPos.y);
			if (dlgHasCaption == TRUE)
				scPos.y += GetSystemMetrics(SM_CYCAPTION);
			SetViewportOrgEx(hDC, scPos.x, scPos.y, NULL);
		}

		/* Draw the caption (if visible) */
		if (dlgHasCaption == TRUE)
		{
			rt.left = 0;
			rt.top = 0 - GetSystemMetrics(SM_CYCAPTION);
			rt.bottom = 0;
			rt.right = DLG2SCR_X(dlgWidth);
			DrawCaption(pseudoHwnd, hDC, &rt, DC_ACTIVE | DC_ICON | DC_TEXT);
			/* To keep things simple, don't draw the system buttons */
			/*rt.left = rt.right - GetSystemMetrics(SM_CXSIZE) -
				GetSystemMetrics(SM_CXSIZEFRAME);
			rt.top += GetSystemMetrics(SM_CYSIZEFRAME);
			rt.right -= GetSystemMetrics(SM_CXSIZEFRAME);
			rt.bottom = rt.top + GetSystemMetrics(SM_CYSIZE);
			DrawFrameControl(hDC, &rt, DFC_CAPTION, DFCS_CAPTIONCLOSE);*/
		}

		/* Draw the dialog client area */
		hBr = GetSysColorBrush(COLOR_3DFACE);
		rt.left = 0;
		rt.top = 0;
		rt.right = DLG2SCR_X(dlgWidth);
		rt.bottom = DLG2SCR_Y(dlgHeight);
		FillRect(hDC, &rt, hBr);
		if (activeCtrl == -1 && clickHit == FALSE)
		{
			/* Draw the selection around the dialog */
			if (dlgHasCaption == TRUE)
				rt.top -= GetSystemMetrics(SM_CYCAPTION);
			DrawSelRect(hDC, &rt);
		}

		{
			unsigned i;
			for (i = 0; i < dlgControls.len; i++)
				DrawDlgItemDispatch(hDC, i);
		}
		EndPaint(hwnd, &ps);
		break;
	}
	case WM_SIZE:
		if (firstSize == TRUE)
		{
			/* Size and position the main and text windows */
			RECT rt;
			int winWidth, winHeight;
			firstSize = FALSE;
			GetWindowRect(hwnd, &rt);
			winWidth = rt.right - rt.left;
			winHeight = rt.bottom - rt.top;
			MoveWindow(hwnd, rt.left, rt.top, winWidth,
				winHeight / 3 * 2, TRUE);
			MoveWindow(textHwnd, rt.left, rt.top + winHeight / 3 * 2,
				winWidth, winHeight / 3, TRUE);
		}
		break;
	/*{
		long winWidth, winHeight;
		long xSmIcon, ySmIcon;
		winWidth = LOWORD(lParam);
		winHeight = HIWORD(lParam);
		xSmIcon = GetSystemMetrics(SM_CXSMICON);
		ySmIcon = GetSystemMetrics(SM_CYSMICON);
		if (dlgPos.x >= (winWidth - xSmIcon))
		{
			dlgPos.x = winWidth - xSmIcon;
			if (dlgPos.x <= 0)
				dlgPos.x = 1;
		}
		if (dlgPos.y >= (winHeight - ySmIcon))
		{
			dlgPos.y = winHeight - ySmIcon;
			if (dlgPos.y <= 0)
				dlgPos.y = 1;
		}
		break;
	}*/
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case M_NEW:
		{
			HDC hDC;
			if (!AskForSave(hwnd))
				break;
			FreeDlgData();
			LoadDialogTemplate(curFilename, TRUE);
			if (dlgHasCaption == TRUE)
				SetWindowText(pseudoHwnd, dlgCaption);
			hDC = GetDC(hwnd);
			SetDlgHFont(hDC);
			UpdateFont(hDC);
			ReleaseDC(hwnd, hDC);
			InvalidateRect(hwnd, NULL, TRUE);
			break;
		}
		case M_OPEN:
			if (!AskForSave(hwnd))
				break;
			curFilename[0] = '\0';
			if (GetFilenameDialog(curFilename, "Open", hwnd, 0))
			{
				FreeDlgData();
				curLine = 0;
				if (LoadDialogTemplate(curFilename, FALSE))
				{
					HDC hDC;
					noFileLoaded = FALSE;
					if (dlgHasCaption == TRUE)
						SetWindowText(pseudoHwnd, dlgCaption);
					hDC = GetDC(hwnd);
					SetDlgHFont(hDC);
					UpdateFont(hDC);
					ReleaseDC(hwnd, hDC);
					fileChanged = FALSE;
					InvalidateRect(hwnd, NULL, TRUE);
				}
				else
				{
					if (curLine != 0)
					{
						char* errorMsg;
						errorMsg = (char*)xmalloc(22 + 11 +
												 strlen(errorDesc) + 1);
						wsprintf(errorMsg, "Parse error on line %i. %s",
							curLine, errorDesc);
						MessageBox(hwnd, errorMsg, NULL,
								   MB_OK | MB_ICONEXCLAMATION);
						xfree(errorMsg);
					}
					else
					{
						MessageBox(hwnd, "Error loading dialog template.",
							NULL, MB_OK | MB_ICONEXCLAMATION);
					}
					noFileLoaded = TRUE;
					/* Load the default dialog template */
					{
						HDC hDC;
						FreeDlgData();
						LoadDialogTemplate(curFilename, TRUE);
						if (dlgHasCaption == TRUE)
							SetWindowText(pseudoHwnd, dlgCaption);
						hDC = GetDC(hwnd);
						SetDlgHFont(hDC);
						UpdateFont(hDC);
						ReleaseDC(hwnd, hDC);
						InvalidateRect(hwnd, NULL, TRUE);
					}
				}
			}
			break;
		case M_SAVE:
			if (noFileLoaded == TRUE)
			{
				if (GetFilenameDialog(curFilename, "Save As", hwnd, 1))
					SaveDialogTemplate(curFilename);
			}
			else
				SaveDialogTemplate(curFilename);
			break;
		case M_SAVEAS:
			if (GetFilenameDialog(curFilename, "Save As", hwnd, 1))
				SaveDialogTemplate(curFilename);
			break;
		case M_EXIT:
			if (!AskForSave(hwnd))
				break;
			DestroyWindow(hwnd);
			break;
		case M_PARSETEXT:
		{
			char* textBuf;
			unsigned textLen;

			fileChanged = TRUE;
			textLen = GetWindowTextLength(textBox);
			textBuf = (char*)malloc(textLen + 1);
			GetWindowText(textBox, textBuf, textLen + 1);
			textLen = SetUnixNlChars(textBuf, textLen);
			/* Add extra space for parser */
			textLen++;
			textBuf = (char*)xrealloc(textBuf, textLen + 1);
			textBuf[textLen-1] = '\0';
			textBuf[textLen] = '\0';
			if (activeCtrl == -1)
			{
				char* oldHead;
				/* Save old header in case of a parse error */
				oldHead = dlgHead;
				dlgHead = NULL;
				xfree(dlgFontFam);
				dlgFontFam = NULL;
				if (!ParseDlgHead(textBuf, textLen))
				{
					char* errorMsg;
					errorMsg = (char*)xmalloc(22 + 11 + strlen(errorDesc) + 1);
					wsprintf(errorMsg, "Parse error on line %i. %s",
						curLine, errorDesc);
					MessageBox(hwnd, errorMsg, NULL,
							   MB_OK | MB_ICONEXCLAMATION);
					xfree(errorMsg);
					/* Set "dlgHead" to the old header */
					dlgHead = oldHead;
				}
				else
				{
					HDC hDC;
					xfree(oldHead); /* The new header was sucessfully set */
					/* Set the caption */
					if (dlgHasCaption == TRUE)
						SetWindowText(pseudoHwnd, dlgCaption);
					/* Set the font automatically */
					hDC = GetDC(hwnd);
					SetDlgHFont(hDC);
					UpdateFont(hDC);
					ReleaseDC(hwnd, hDC);
				}
			}
			else
			{
				DlgItem* pCtrl;
				pCtrl = &dlgControls.d[activeCtrl];
				curPos = 0;
				/* Delete dynamic allocations in control before parsing */
				xfree(pCtrl->id);
				pCtrl->id = NULL;
				xfree(pCtrl->style);
				pCtrl->style = NULL;
				xfree(pCtrl->exStyle);
				pCtrl->exStyle = NULL;
				if (!ParseControl(textBuf, textLen, activeCtrl))
				{
					char* errorMsg;
					errorMsg = (char*)xmalloc(29 + strlen(errorDesc) + 1);
					wsprintf(errorMsg, "Parse error in control line. %s",
						errorDesc);
					MessageBox(hwnd, errorMsg, NULL,
							   MB_OK | MB_ICONEXCLAMATION);
					xfree(errorMsg);
					/* Allocate empty strings to avoid crashes */
					pCtrl->id = (char*)xmalloc(1);
					pCtrl->id[0] = '\0';
					pCtrl->style = (char*)xmalloc(1);
					pCtrl->style[0] = '\0';
					pCtrl->exStyle = (char*)xmalloc(1);
					pCtrl->exStyle[0] = '\0';
				}
			}
			xfree(textBuf);
			InvalidateRect(hwnd, NULL, TRUE);
			break;
		}
		case M_ADDCTRL:
		{
			char* textBuf;
			unsigned textLen;

			fileChanged = TRUE;
			textLen = GetWindowTextLength(textBox);
			textBuf = (char*)xmalloc(textLen + 1);
			GetWindowText(textBox, textBuf, textLen + 1);
			textLen = SetUnixNlChars(textBuf, textLen);

			curPos = 0;
			/* Add extra space for parser */
			textLen++;
			textBuf = (char*)xrealloc(textBuf, textLen + 1);
			textBuf[textLen-1] = '\0';
			textBuf[textLen] = '\0';
			if (!ParseControl(textBuf, textLen, dlgControls.len))
			{
				char* errorMsg;
				unsigned newCtrl;
				errorMsg = (char*)xmalloc(29 + strlen(errorDesc) + 1);
				wsprintf(errorMsg, "Parse error in control line. %s",
					errorDesc);
				MessageBox(hwnd, errorMsg, NULL, MB_OK | MB_ICONEXCLAMATION);
				xfree(errorMsg);
				/* Delete dynamic allocations, if any */
				newCtrl = dlgControls.len;
				xfree(dlgControls.d[newCtrl].id);
				dlgControls.d[newCtrl].id = NULL;
				xfree(dlgControls.d[newCtrl].style);
				dlgControls.d[newCtrl].style = NULL;
				xfree(dlgControls.d[newCtrl].exStyle);
				dlgControls.d[newCtrl].exStyle = NULL;
			}
			else
			{
				activeCtrl = dlgControls.len;
				EA_ADD(DlgItem, dlgControls);
			}

			xfree(textBuf);
			InvalidateRect(hwnd, NULL, TRUE);
			break;
		}
		case M_DELCTRL:
			if (activeCtrl != -1)
			{
				DlgItem* pCtrl;
				pCtrl = &dlgControls.d[activeCtrl];
				/* Delete dynamic allocations */
				xfree(pCtrl->id);
				pCtrl->id = NULL;
				xfree(pCtrl->style);
				pCtrl->style = NULL;
				xfree(pCtrl->exStyle);
				pCtrl->exStyle = NULL;
				EA_REMOVE(DlgItem, dlgControls, activeCtrl);
				activeCtrl = -1;
				InvalidateRect(hwnd, NULL, TRUE);
			}
			break;
		case M_FONT:
		{
			CHOOSEFONT cf;
			LOGFONT logFnt;
			ZeroMemory(&cf, sizeof(CHOOSEFONT));
			GetObject(dlgFont, sizeof(LOGFONT), &logFnt);
			cf.lStructSize = sizeof(CHOOSEFONT);
			cf.hwndOwner = hwnd;
			cf.lpLogFont = &logFnt;
			cf.Flags = CF_SCREENFONTS;
			if (dlgFont != NULL)
				cf.Flags |= CF_INITTOLOGFONTSTRUCT;
			if (ChooseFont(&cf))
			{
				HDC hDC;
				DeleteObject(dlgFont);
				dlgFont = CreateFontIndirect(cf.lpLogFont);
				hDC = GetDC(hwnd);
				SelectObject(hDC, dlgFont);
				UpdateFont(hDC);
				ReleaseDC(hwnd, hDC);
				InvalidateRect(hwnd, NULL, TRUE);
			}
			break;
		}
		case M_SHOW_T_WIN:
			ShowWindow(textHwnd, SW_SHOWNORMAL);
			SetActiveWindow(textHwnd);
			break;
		case M_T_WIN_FONT:
		{
			CHOOSEFONT cf;
			LOGFONT logFnt;
			ZeroMemory(&cf, sizeof(CHOOSEFONT));
			GetObject(teFont, sizeof(LOGFONT), &logFnt);
			cf.lStructSize = sizeof(CHOOSEFONT);
			cf.hwndOwner = hwnd;
			cf.lpLogFont = &logFnt;
			cf.Flags = CF_SCREENFONTS;
			if (dlgFont != NULL)
				cf.Flags |= CF_INITTOLOGFONTSTRUCT;
			if (ChooseFont(&cf))
			{
				DeleteObject(teFont);
				teFont = CreateFontIndirect(cf.lpLogFont);
				SendMessage(textBox, WM_SETFONT,
							(WPARAM)teFont, (LPARAM)TRUE);
			}
			break;
		}
		case M_ABOUT:
			DialogBox(g_hInstance, (LPCTSTR)ABOUTBOX, hwnd, AboutBoxProc);
			break;
		}
		break;
	case WM_KEYDOWN:
		switch (wParam)
		{
		case VK_DELETE:
		case VK_BACK:
			if (activeCtrl != -1)
			{
				DlgItem* pCtrl;
				pCtrl = &dlgControls.d[activeCtrl];
				/* Delete dynamic allocations */
				xfree(pCtrl->id);
				pCtrl->id = NULL;
				xfree(pCtrl->style);
				pCtrl->style = NULL;
				xfree(pCtrl->exStyle);
				pCtrl->exStyle = NULL;
				EA_REMOVE(DlgItem, dlgControls, activeCtrl);
				activeCtrl = -1;
				InvalidateRect(hwnd, NULL, TRUE);
			}
			break;
		case VK_ESCAPE:
			if (clickHit == TRUE)
			{
				CancelDrag();
				clickHit = FALSE;
				ClipCursor(NULL);
				InvalidateRect(hwnd, NULL, TRUE);
			}
			break;
		/* Keyboard interface for graphical editing */
		case VK_TAB:
			/* Invalidate the area at the old control dimensions */
			InvalSelItem(hwnd);

			if (GetKeyState(VK_SHIFT) & 0x8000)
			{
				if (activeCtrl == -1)
					activeCtrl = dlgControls.len - 1;
				else
					activeCtrl--;
			}
			else
			{
				if (activeCtrl == dlgControls.len  - 1)
					activeCtrl = -1;
				else
					activeCtrl++;
			}
			InvalSelItem(hwnd);
			UpdateTextWindow();
			break;
		}
		KbdMoveProc(hwnd, wParam);
		break;
	case WM_LBUTTONDOWN:
		ProcLBtnDown(hwnd, lParam);
		break;
	case WM_MOUSEMOVE:
		ProcMouseMove(hwnd, lParam);
		break;
	case WM_LBUTTONUP:
		EndDlgClick(hwnd);
		clickHit = FALSE;
		curDragHand = -1;
		break;
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			EndDlgClick(hwnd);
			clickHit = FALSE;
			curDragHand = -1;
		}
		break;
	case WM_SETCURSOR:
	{
		/* Do not change the cursor type while dragging */
		if (clickHit == FALSE)
			DragHandleTest(hwnd);
		switch (curDragHand)
		{
		case 0: /* top-left */
		case 4: /* bottom-right */
			SetCursor(LoadCursor(NULL, IDC_SIZENWSE));
			return TRUE;
		case 1: /* top */
		case 5: /* bottom */
			SetCursor(LoadCursor(NULL, IDC_SIZENS));
			return TRUE;
		case 2: /* top-right */
		case 6: /* bottom-left */
			SetCursor(LoadCursor(NULL, IDC_SIZENESW));
			return TRUE;
		case 3: /* right */
		case 7: /* left */
			SetCursor(LoadCursor(NULL, IDC_SIZEWE));
			return TRUE;
		}
		break;
	}

	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK TextWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
								LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CREATE:
	{
		CREATESTRUCT* cs;
		HDC hDC;
		cs = (CREATESTRUCT*)lParam;
		textBox = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", NULL,
			WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL |
			ES_LEFT | ES_MULTILINE | ES_AUTOHSCROLL | ES_WANTRETURN,
			0, 0, cs->cx, cs->cy, hwnd,
			(HMENU)1, g_hInstance, NULL);
		if (textBox == NULL)
			return -1;
		hDC = GetDC(hwnd);
		teFont = CreateFont(-MulDiv(10, GetDeviceCaps(hDC, LOGPIXELSY), 72),
			0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, ANSI_CHARSET,
			OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
			FIXED_PITCH | FF_DONTCARE, "Courier New");
		ReleaseDC(hwnd, hDC);
		SendMessage(textBox, WM_SETFONT, (WPARAM)teFont, (LPARAM)FALSE);
		break;
	}
	case WM_CLOSE:
		ShowWindow(hwnd, SW_HIDE);
		return 0;
	case WM_DESTROY:
		DestroyWindow(textBox);
		DeleteObject(teFont);
		textHwnd = NULL;
		break;
	case WM_SIZE:
		MoveWindow(textBox, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
		break;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

INT_PTR CALLBACK AboutBoxProc(HWND hDlg, UINT uMsg, WPARAM wParam,
							  LPARAM lParam)
{
	HWND hwndOwner;
	RECT rc, rcDlg, rcOwner;

	switch (uMsg)
	{
	case WM_INITDIALOG:
		/* Center the dialog in the parent window */
		/* Get the owner window and dialog box rectangles */
		if ((hwndOwner = GetParent(hDlg)) == NULL)
		{
			hwndOwner = GetDesktopWindow();
		}

		GetWindowRect(hwndOwner, &rcOwner);
		GetWindowRect(hDlg, &rcDlg);
		CopyRect(&rc, &rcOwner);

		/* Offset the owner and dialog box rectangles so that
		   right and bottom values represent the width and
		   height, and then offset the owner again to discard
		   space taken up by the dialog box. */
		OffsetRect(&rcDlg, -rcDlg.left, -rcDlg.top);
		OffsetRect(&rc, -rc.left, -rc.top);
		OffsetRect(&rc, -rcDlg.right, -rcDlg.bottom);

		/* The new position is the sum of half the remaining
		   space and the owner's original position. */
		SetWindowPos(hDlg,
			HWND_TOP,
			rcOwner.left + (rc.right / 2),
			rcOwner.top + (rc.bottom / 2),
					 0, 0,          /* ignores size arguments */
			SWP_NOSIZE);
		return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return TRUE;
		}
		break;
	}
	return FALSE;
}

#define SKIP_WHITESPACE() \
	while (curPos < dataSize && \
		(buffer[curPos] == ' ' || buffer[curPos] == '\t')) \
		curPos++;
#define CHECK_CHAR(chcode) (buffer[curPos] == chcode)

BOOL LoadDialogTemplate(char* filename, BOOL useNewTmpl)
{
	char* buffer;
	unsigned fileSize;
	unsigned dataSize;
	FILE* fp;

	if (useNewTmpl == FALSE)
	{
		/* Read the file */
		fp = fopen(filename, "rb");
		if (fp == NULL)
			return FALSE;
		fseek(fp, 0, SEEK_END);
		fileSize = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		buffer = (char*)xmalloc(fileSize + 1);
		if (buffer == NULL)
		{
			fclose(fp);
			return FALSE;
		}
		fread(buffer, fileSize, 1, fp);
		buffer[fileSize] = '\0';
		fclose(fp);
	}
	else
	{
		/* Initialize edit variables */
		strcpy(curFilename, "Untitled.dlg");
		noFileLoaded = TRUE;
		fileChanged = FALSE;
		dlgHead = NULL;
		dlgFontFam = NULL;
		activeCtrl = -1;

		/* Load a default template */
		{
			HRSRC hRes;
			HGLOBAL hResData;
			char* resMem;
			hRes = FindResource(NULL, (LPCTSTR)NEW_TEMPLATE, RT_RCDATA);
			hResData = LoadResource(NULL, hRes);
			fileSize = SizeofResource(NULL, hRes);
			resMem = (char*)LockResource(hResData);
			/* Create a writeable copy of the memory */
			buffer = (char*)xmalloc(fileSize + 1);
			strncpy(buffer, resMem, fileSize);
			buffer[fileSize] = '\0';
		}
	}

	/* Since we read the whole file this way, we will have to properly
	   format newlines. */
	dataSize = SetUnixNlChars(buffer, fileSize);

	/* Parse the file */
	EA_INIT(DlgItem, dlgControls, 16);
	if (!ParseDlgHead(buffer, dataSize))
	{
		xfree(buffer);
		return FALSE;
	}
	while (curPos < dataSize)
	{
		unsigned lastPos;
		if (!ParseControl(buffer, dataSize, dlgControls.len))
		{
			/* Do fully safe cleanup */
			EA_ADD(DlgItem, dlgControls);
			xfree(buffer);
			FreeDlgData();
			return FALSE;
		}
		EA_ADD(DlgItem, dlgControls);
		/* Check for "END" or '}' */
		lastPos = curPos;
		SKIP_WHITESPACE();
		if (curPos >= fileSize)
		{
			/* Do fully safe cleanup */
			EA_ADD(DlgItem, dlgControls);
			xfree(buffer);
			FreeDlgData();
			return FALSE;
		}
		if ((CHECK_CHAR('}') || strncmp("END", &buffer[curPos], 3) == 0))
			break;
		else
			curPos = lastPos;
	}
	xfree(buffer);
	return TRUE;
}

#undef SKIP_WHITESPACE
#undef CHECK_CHAR

BOOL SaveDialogTemplate(char* filename)
{
	FILE* fp;
	/* Temporary variables */
	unsigned i;

	fp = fopen(filename, "wt");
	if (fp == NULL)
		return FALSE;

	/* Write the header */
	fputs(dlgHead, fp);
	/* Write the controls */
	for (i = 0; i < dlgControls.len; i++)
	{
		char* ctrlLine;
		ctrlLine = FmtControlText(i);
		fputs(ctrlLine, fp);
		xfree(ctrlLine);
	}
	/* Write the end marker */
	if (dlgHead[strlen(dlgHead)-2] == '{')
		fputs("}\n", fp);
	else
		fputs("END\n", fp);
	fclose(fp);

	/* Update flags */
	fileChanged = FALSE;
	return TRUE;
}

void SetDlgHFont(HDC hDC)
{
	DeleteObject(dlgFont);
	dlgFont = CreateFont(
		-MulDiv(dlgPointSize, GetDeviceCaps(hDC, LOGPIXELSY), 72),
		0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, ANSI_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
		DEFAULT_PITCH | FF_DONTCARE, dlgFontFam);
	SelectObject(hDC, dlgFont);
}

void UpdateTextWindow()
{
	if (activeCtrl == -1)
	{
		char* winBuff;
		FmtDlgHeader();
		winBuff = GenWinNlChars(dlgHead, strlen(dlgHead));
		SetWindowText(textBox, winBuff);
		xfree(winBuff);
	}
	else
	{
		char* newText;
		unsigned textLen;
		newText = FmtControlText(activeCtrl);
		/* Make sure that the line endings are in CR+LF form */
		textLen = strlen(newText);
		newText = (char*)xrealloc(newText, textLen + 2);
		newText[textLen-1] = '\r';
		newText[textLen] = '\n';
		textLen++;
		newText[textLen] = '\0';
		SetWindowText(textBox, newText);
		xfree(newText);
	}
}

void KbdMoveProc(HWND hwnd, WPARAM wParam)
{
	DlgItem* pCtrl;
	int stride;
	BOOL noModify;

	/* Invalidate the area at the old control dimensions */
	InvalSelItem(hwnd);

	if (activeCtrl != -1)
		pCtrl = &dlgControls.d[activeCtrl];
	if (GetKeyState(VK_CONTROL) & 0x8000)
		stride = 1;
	else
		stride = 8;
	noModify = FALSE;
	if (activeCtrl == -1)
	{
		if (GetKeyState(VK_SHIFT) & 0x8000)
		{
			switch (wParam)
			{
			case VK_UP: dlgHeight -= stride; break;
			case VK_DOWN: dlgHeight += stride; break;
			case VK_LEFT: dlgWidth -= stride; break;
			case VK_RIGHT: dlgWidth += stride; break;
			default: noModify = TRUE; break;
			}
		}
		else
		{
			switch (wParam)
			{
			case VK_UP: dlgPos.y -= stride; break;
			case VK_DOWN: dlgPos.y += stride; break;
			case VK_LEFT: dlgPos.x -= stride; break;
			case VK_RIGHT: dlgPos.x += stride; break;
			default: noModify = TRUE; break;
			}
		}
	}
	else
	{
		if (GetKeyState(VK_SHIFT) & 0x8000)
		{
			switch (wParam)
			{
			case VK_UP: pCtrl->cy -= stride; break;
			case VK_DOWN: pCtrl->cy += stride; break;
			case VK_LEFT: pCtrl->cx -= stride; break;
			case VK_RIGHT: pCtrl->cx += stride; break;
			default: noModify = TRUE; break;
			}
		}
		else
		{
			switch (wParam)
			{
			case VK_UP: pCtrl->y -= stride; break;
			case VK_DOWN: pCtrl->y += stride; break;
			case VK_LEFT: pCtrl->x -= stride; break;
			case VK_RIGHT: pCtrl->x += stride; break;
			default: noModify = TRUE; break;
			}
		}
	}
	if (noModify == FALSE)
	{
		fileChanged = TRUE;
		InvalSelItem(hwnd);
		UpdateTextWindow();
	}
}
