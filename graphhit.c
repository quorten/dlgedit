/* Graphical dialog code, intended to assist hit-testing and rendering. */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string.h>

#include "dlgedit.h"
#include "tmplparser.h"
#include "ufsys.h"

#include "graphhit.h"

/* Rendering variables */
HFONT dlgFont;
long fontHeight;
long avgCharWidth;
long dlgBaseX, dlgBaseY; /* Dialog base units */

/* Hit-testing variables */
BOOL clickHit = FALSE;
POINT lastDlgPos; /* Specifies dialog position before being dragged */
int activeCtrl; /* The control that has the selection, -1 for the window */
RECT dragHandles[8]; /* Must be initialized to zero (default for globals) */
int curDragHand = -1; /* -1 means no handle is being hovered */

static POINT downPos;
static POINT lastMovePos;
/* This is actually not a (l, t, r, b) rectangle but (x, y, w, h) */
static RECT origRect;

/********************************************************************\
 * Hit-testing														*
\********************************************************************/

void ProcLBtnDown(HWND hwnd, LPARAM lParam)
{
	POINT pt;
	RECT rt;
	pt.x = (short)LOWORD(lParam);
	pt.y = (short)HIWORD(lParam);

	/* Drag a selection handle if applicable */
	if (curDragHand != -1)
	{
		clickHit = TRUE;
		downPos.x = pt.x;
		downPos.y = pt.y;
		if (activeCtrl == -1)
		{
			origRect.left = dlgPos.x;
			origRect.top = dlgPos.y;
			origRect.right = dlgWidth;
			origRect.bottom = dlgHeight;
		}
		else
		{
			DlgItem* pCtrl;
			pCtrl = &dlgControls.d[activeCtrl];
			origRect.left = pCtrl->x;
			origRect.top = pCtrl->y;
			origRect.right = pCtrl->cx;
			origRect.bottom = pCtrl->cy;
		}
		WindowCursorClip(hwnd);
		return;
	}

	/* Check if the titlebar was clicked on */
	if (dlgHasCaption == TRUE)
	{
		rt.left = DLG2SCR_X(dlgPos.x);
		rt.top = DLG2SCR_Y(dlgPos.y);
		rt.right = rt.left + DLG2SCR_X(dlgWidth);
		rt.bottom = rt.top + GetSystemMetrics(SM_CYCAPTION);
		if (PtInRect(&rt, pt))
		{
			/* Make the dialog active */
			activeCtrl = -1;
			InvalidateRect(hwnd, NULL, TRUE);
			UpdateTextWindow();
			/* Drag the dialog window */
			clickHit = TRUE;
			downPos.x = pt.x;
			downPos.y = pt.y;
			lastMovePos.x = (short)LOWORD(lParam);
			lastMovePos.y = (short)HIWORD(lParam);
			lastDlgPos.x = dlgPos.x;
			lastDlgPos.y = dlgPos.y;
			WindowCursorClip(hwnd);
			return;
		}
	}

	/* Check if a control was clicked on */
	{
		unsigned i;
		BOOL hitTest;
		hitTest = FALSE;
		for (i = 0; i < dlgControls.len; i++)
		{
			/* Get the item rectangle */
			rt.left = dlgControls.d[i].x;
			rt.top = dlgControls.d[i].y;
			rt.right = rt.left + dlgControls.d[i].cx;
			rt.bottom = rt.top + dlgControls.d[i].cy;
			DlgUnitMap(&rt);
			/* Translate to absolute screen coordinates */
			rt.left += DLG2SCR_X(dlgPos.x);
			rt.top += DLG2SCR_Y(dlgPos.y);
			rt.right += DLG2SCR_X(dlgPos.x);
			rt.bottom += DLG2SCR_Y(dlgPos.y);
			if (dlgHasCaption == TRUE)
			{
				rt.top += GetSystemMetrics(SM_CYCAPTION);
				rt.bottom += GetSystemMetrics(SM_CYCAPTION);
			}
			if (PtInRect(&rt, pt))
			{
				activeCtrl = i;
				hitTest = TRUE;
				lastDlgPos.x = dlgControls.d[i].x;
				lastDlgPos.y = dlgControls.d[i].y;
				/* Keep going so that we get the control on top */
			}
		}
		if (hitTest == TRUE)
		{
			/* Possibly drag the control */
			clickHit = TRUE;
			downPos.x = pt.x;
			downPos.y = pt.y;
			InvalidateRect(hwnd, NULL, TRUE);
			UpdateTextWindow();
			WindowCursorClip(hwnd);
			return;
		}
	}

	/* As a last resort, the dialog can be selected */
	rt.left = DLG2SCR_X(dlgPos.x);
	rt.top = DLG2SCR_Y(dlgPos.y);
	rt.right = rt.left + DLG2SCR_X(dlgWidth);
	rt.bottom = rt.top + DLG2SCR_Y(dlgHeight);
	if (dlgHasCaption == TRUE)
	{
		rt.top += GetSystemMetrics(SM_CYCAPTION);
		rt.bottom += GetSystemMetrics(SM_CYCAPTION);
	}
	if (PtInRect(&rt, pt))
	{
		activeCtrl = -1;
		InvalidateRect(hwnd, NULL, TRUE);
		UpdateTextWindow();
	}
}

void WindowCursorClip(HWND hwnd)
{
	POINT pt;
	RECT rt;
	GetClientRect(hwnd, &rt);
	pt.x = 0;
	pt.y = 0;
	ClientToScreen(hwnd, &pt);
	rt.left += pt.x; rt.right += pt.x;
	rt.top += pt.y; rt.bottom += pt.y;
	ClipCursor(&rt);
}

void ProcMouseMove(HWND hwnd, LPARAM lParam)
{
	if (clickHit == TRUE && curDragHand != -1)
	{
		POINT dlgUnitDelta;
		/* This is actually not a (l, t, r, b) rectangle but (x, y, w, h) */
		RECT newCoords;
		unsigned i;

		fileChanged = TRUE;
		dlgUnitDelta.x = (short)LOWORD(lParam) - downPos.x;
		dlgUnitDelta.y = (short)HIWORD(lParam) - downPos.y;
		dlgUnitDelta.x = MulDiv(dlgUnitDelta.x, 4, dlgBaseX);
		dlgUnitDelta.y = MulDiv(dlgUnitDelta.y, 8, dlgBaseY);

		/* Invalidate the area at the old control dimensions */
		InvalSelItem(hwnd);

		/* Drag handle indices start at the top-left and proceed
		   clockwise.  See DrawSelRect() for a diagram. */
		i = curDragHand;
		if (activeCtrl == -1)
		{
			newCoords.left = dlgPos.x;
			newCoords.top = dlgPos.y;
			newCoords.right = dlgWidth;
			newCoords.bottom = dlgHeight;
		}
		else
		{
			DlgItem* pCtrl;
			pCtrl = &dlgControls.d[activeCtrl];
			newCoords.left = pCtrl->x;
			newCoords.top = pCtrl->y;
			newCoords.right = pCtrl->cx;
			newCoords.bottom = pCtrl->cy;
		}
		if (i == 0 || i == 6 || i == 7)
		{
			newCoords.right = origRect.right - dlgUnitDelta.x;
			newCoords.left = origRect.left + dlgUnitDelta.x;
		}
		if (i == 2 || i == 3 || i == 4)
			newCoords.right = origRect.right + dlgUnitDelta.x;
		if (i == 0 || i == 1 || i == 2)
		{
			newCoords.bottom = origRect.bottom - dlgUnitDelta.y;
			newCoords.top = origRect.top + dlgUnitDelta.y;
		}
		if (i == 4 || i == 5 || i == 6)
			newCoords.bottom = origRect.bottom + dlgUnitDelta.y;

		if (activeCtrl == -1)
		{
			dlgPos.x = newCoords.left;
			dlgPos.y = newCoords.top;
			dlgWidth = newCoords.right;
			dlgHeight = newCoords.bottom;
		}
		else
		{
			DlgItem* pCtrl;
			pCtrl = &dlgControls.d[activeCtrl];
			pCtrl->x = newCoords.left;
			pCtrl->y = newCoords.top;
			pCtrl->cx = newCoords.right;
			pCtrl->cy = newCoords.bottom;
		}
		InvalSelItem(hwnd);
	}
	else if (clickHit == TRUE && activeCtrl == -1)
	{
		/*RECT updRect; Update clipping does not work when dlgPos < 0 */
		POINT pixMove;
		/* The the bits to scroll before further calculations */
		/*updRect.left = dlgPos.x;
		  updRect.top = dlgPos.y;
		  updRect.right = updRect.left + dlgWidth;
		  updRect.bottom = updRect.top + dlgHeight;
		  DlgUnitMap(&updRect);
		  if (dlgHasCaption == TRUE)
		  updRect.bottom += GetSystemMetrics(SM_CYCAPTION);*/
		fileChanged = TRUE;
		dlgPos.x = lastDlgPos.x +
			MulDiv((short)LOWORD(lParam) - downPos.x, 4, dlgBaseX);
		dlgPos.y = lastDlgPos.y +
			MulDiv((short)HIWORD(lParam) - downPos.y, 8, dlgBaseY);
		/* Due to dialog unit rounding, scrolling can be a bit
		   complicatated. */
		/* First we convert the current dialog position to screen units */
		pixMove.x = DLG2SCR_X(dlgPos.x);
		pixMove.y = DLG2SCR_Y(dlgPos.y);
		/* Then we calculate the dialog displacement in screen units */
		pixMove.x -= DLG2SCR_X(lastDlgPos.x);
		pixMove.y -= DLG2SCR_Y(lastDlgPos.y);
		/* Then we add this displacement to the position where the
		   mouse was clicked for dragging */
		pixMove.x += downPos.x;
		pixMove.y += downPos.y;
		/*if (dlgPos.x < 0 || dlgPos.y < 0)
		  {
		  /* Be on the safe side of the scroll rectangle, otherwise
		  the full dialog bits sometimes don't get scrolled. *
		  updRect.right += dlgBaseX / 4;
		  updRect.bottom += dlgBaseY / 8;
		  }*/
		/* Finally, we scroll the window based off of the difference
		   between the current drawn position and the last drawn
		   position. */
		ScrollWindowEx(hwnd,
					   pixMove.x - lastMovePos.x,
					   pixMove.y - lastMovePos.y,
					   NULL, NULL, NULL, NULL, SW_ERASE | SW_INVALIDATE);
		lastMovePos.x = pixMove.x;
		lastMovePos.y = pixMove.y;
	}
	else if (clickHit == TRUE)
	{
		DlgItem* pCtrl;
		pCtrl = &dlgControls.d[activeCtrl];

		fileChanged = TRUE;
		/* Invalidate the area at the old control dimensions */
		InvalSelItem(hwnd);

		pCtrl->x = lastDlgPos.x +
			MulDiv((short)LOWORD(lParam) - downPos.x, 4, dlgBaseX);
		pCtrl->y = lastDlgPos.y +
			MulDiv((short)HIWORD(lParam) - downPos.y, 8, dlgBaseY);

		/* Invalidate the new area */
		InvalSelItem(hwnd);
	}
}

/* Tests which drag rectangle the cursor is in. */
void DragHandleTest(HWND hwnd)
{
	unsigned i;
	RECT rt;
	POINT pt;

	GetCursorPos(&pt);
	ScreenToClient(hwnd, &pt);
	/* Drag retangles start at the top-left corner and proceed
	   clockwise */
	for (i = 0; i < 8; i++)
	{
		CopyRect(&rt, &dragHandles[i]);
		if (dlgHasCaption == TRUE)
		{
			rt.top += GetSystemMetrics(SM_CYCAPTION);
			rt.bottom += GetSystemMetrics(SM_CYCAPTION);
		}
		rt.left += DLG2SCR_X(dlgPos.x);
		rt.top += DLG2SCR_Y(dlgPos.y);
		rt.right += DLG2SCR_X(dlgPos.x);
		rt.bottom += DLG2SCR_Y(dlgPos.y);
		if (PtInRect(&rt, pt))
		{
			curDragHand = i;
			return;
		}
	}
	/* If the cursor is not on a drag handle, then reset curDragHand */
	curDragHand = -1;
	return;
}

void EndDlgClick(HWND hwnd)
{
	if (clickHit == TRUE)
	{
		ClipCursor(NULL);
		InvalidateRect(hwnd, NULL, FALSE);
		UpdateTextWindow();
	}
}

void CancelDrag()
{
	if (curDragHand == -1)
	{
		if (activeCtrl == -1)
		{
			dlgPos.x = lastDlgPos.x;
			dlgPos.y = lastDlgPos.y;
		}
		else
		{
			DlgItem* pCtrl;
			pCtrl = &dlgControls.d[activeCtrl];
			pCtrl->x = lastDlgPos.x;
			pCtrl->y = lastDlgPos.y;
		}
	}
	else
	{
		if (activeCtrl == -1)
		{
			dlgPos.x = origRect.left;
			dlgPos.y = origRect.top;
			dlgWidth = origRect.right;
			dlgHeight = origRect.bottom;
		}
		else
		{
			DlgItem* pCtrl;
			pCtrl = &dlgControls.d[activeCtrl];
			pCtrl->x = origRect.left;
			pCtrl->y = origRect.top;
			pCtrl->cx = origRect.right;
			pCtrl->cy = origRect.bottom;
		}
	}
}

void UpdateFont(HDC hDC)
{
	TEXTMETRIC tm;
	SIZE size;
	SelectObject(hDC, dlgFont);
	GetTextMetrics(hDC, &tm);
	fontHeight = tm.tmHeight;
	/* GetTextMetrics() only gives an approximation for the average
	   character width, so we must calculate the actual average
	   character width by this method. */
	GetTextExtentPoint32(hDC,
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz",
		52, &size);
	avgCharWidth = (size.cx / 26 + 1) / 2;
	dlgBaseX = avgCharWidth;
	dlgBaseY = fontHeight;
}

/********************************************************************\
 * Rendering														*
\********************************************************************/

void DrawSelRect(HDC hDC, RECT* rt)
{
	RECT rt2;
	HRGN bordRgn;
	HRGN innerRgn;
	HBITMAP hBm;
	HBRUSH fillBrush;
	int cxEdge, cyEdge;
	/* "hatchBitmap" is a 3x3 bitmap.  The scanlines must be word
	   aligned. */
	/*//const char hatchBitmap[6] =
	  {'\xC0', '\x00', '\xA0', '\x00', '\x60', '\x00'};
								/* 110. .... 101. .... 011. .... */
	/* There's a bug on Windows 98 that forces pattern bitmaps to be
	   at least 8x8 in size, no smaller sizes will work. */
	const char hatchBitmap[16] =
		{'\xDD', '\x00', '\xBB', '\x00', '\x77', '\x00', '\xEE', '\x00',
		 '\xDD', '\x00', '\xBB', '\x00', '\x77', '\x00', '\xEE', '\x00'};
	/* Scanlines go from bottom to top.  The following comment is
	   a graphical top-to-bottom representation. */
	/* 11011101 */
	/* 10111011 */
	/* 01110111 */
	/* 11101110 */
	/* 11011101 */
	/* 10111011 */
	/* 01110111 */
	/* 11101110 */

	/* Reset necessary variables */
	if (clickHit == FALSE)
		curDragHand = -1;

	/* Caclulate the outer edges */
	CopyRect(&rt2, rt);
	cxEdge = GetSystemMetrics(SM_CXEDGE);
	cyEdge = GetSystemMetrics(SM_CYEDGE);
	InflateRect(&rt2, cxEdge * 2, cyEdge * 2);

	/* Calculate the region of the selection rectangle */
	bordRgn = CreateRectRgnIndirect(&rt2);
	innerRgn = CreateRectRgnIndirect(rt);
	CombineRgn(bordRgn, bordRgn, innerRgn, RGN_DIFF);
	DeleteObject(innerRgn);

	/* Draw the region */
	hBm = CreateBitmap(8, 8, 1, 1, &hatchBitmap);
	fillBrush = CreatePatternBrush(hBm);
	/*//fillBrush = CreateHatchBrush(HS_FDIAGONAL,
	  GetSysColor(COLOR_WINDOWTEXT));*/
	FillRgn(hDC, bordRgn, fillBrush);
	DeleteObject(fillBrush);
	DeleteObject(hBm);
	DeleteObject(bordRgn);

	/* Draw the 8 drag handles */
	{
		unsigned topEdge, leftEdge, bottomEdge, rightEdge, horzMid, vertMid;
		unsigned i;
		RECT rtDrag;
		topEdge = rt2.top;
		leftEdge = rt2.left;
		bottomEdge = rt->bottom;
		rightEdge = rt->right;
		horzMid = (rt2.right - rt2.left) / 2 + rt2.left - cxEdge;
		vertMid = (rt2.bottom - rt2.top) / 2 + rt2.top - cyEdge;
		fillBrush = GetSysColorBrush(COLOR_ACTIVECAPTION);
		/* Start at the top left corner and proceed clockwise */
		/*
		         top-left         top         top-right
		            +--------------+--------------+
		            |0             1             2|
		            |                             |
		       left +7                           3+ right
		            |                             |
		            |6             5             4|
		            +--------------+--------------+
		       bottom-left       bottom      bottom-right
		*/
		for (i = 0; i < 8; i++)
		{
			if (i == 0 || i == 6 || i == 7)
				rtDrag.left = leftEdge;
			if (i == 2 || i == 3 || i == 4)
				rtDrag.left = rightEdge;
			if (i == 0 || i == 1 || i == 2)
				rtDrag.top = topEdge;
			if (i == 4 || i == 5 || i == 6)
				rtDrag.top = bottomEdge;
			if (i == 1 || i == 5)
				rtDrag.left = horzMid;
			if (i == 3 || i == 7)
				rtDrag.top = vertMid;
			rtDrag.right = rtDrag.left + cxEdge * 2;
			rtDrag.bottom = rtDrag.top + cyEdge * 2;
			CopyRect(&dragHandles[i], &rtDrag);
			FillRect(hDC, &rtDrag, fillBrush);
		}
	}
}

void InvalSelItem(HWND hwnd)
{
	RECT rt;
	/* Translate dialog coordinates to screen coordinates */
	if (activeCtrl == -1)
	{
		/* We don't actually support partial rendering of the entire dialog */
		InvalidateRect(hwnd, NULL, TRUE);
		return;
		/*rt.left = dlgPos.x;
		rt.top = dlgPos.y;
		rt.right = rt.left + dlgWidth;
		rt.bottom = rt.top + dlgHeight;
		DlgUnitMap(&rt);
		if (dlgHasCaption == TRUE)
			rt.bottom += GetSystemMetrics(SM_CYCAPTION);*/
	}
	else
	{
		DlgItem* pCtrl;
		POINT scrOffset;
		pCtrl = &dlgControls.d[activeCtrl];
		rt.left = pCtrl->x;
		rt.top = pCtrl->y;
		rt.right = rt.left + pCtrl->cx;
		rt.bottom = rt.top + pCtrl->cy;
		DlgUnitMap(&rt);

		/* Add the dialog position separately to avoid rounding errors */
		scrOffset.x = DLG2SCR_X(dlgPos.x);
		scrOffset.y = DLG2SCR_Y(dlgPos.y);
		if (dlgHasCaption == TRUE)
			scrOffset.y += GetSystemMetrics(SM_CYCAPTION);
		rt.left += scrOffset.x; rt.top += scrOffset.y;
		rt.right += scrOffset.x; rt.bottom += scrOffset.y;
	}

	/* Caclulate the outer edges */
	{
		int cxEdge, cyEdge;
		/* Add safe zone to negative rectangles */
		cxEdge = GetSystemMetrics(SM_CXEDGE);
		if (rt.right < rt.left)
			cxEdge = -cxEdge;
		cyEdge = GetSystemMetrics(SM_CYEDGE);
		if (rt.bottom < rt.top)
			cyEdge = -cyEdge;
		InflateRect(&rt, cxEdge * 2, cyEdge * 2);
	}

	InvalidateRect(hwnd, &rt, TRUE);
}

void DrawDlgItemDispatch(HDC hDC, unsigned ctrlNum)
{
	RECT rt;
	DlgItem* pCtrl;

	pCtrl = &dlgControls.d[ctrlNum];
	rt.left = pCtrl->x;
	rt.top = pCtrl->y;
	rt.right = rt.left + pCtrl->cx;
	rt.bottom = rt.top + pCtrl->cy;

	SetBkMode(hDC, TRANSPARENT);
	/* Drawn controls are grouped by drawing style */
	switch (pCtrl->rendClass)
	{
	case 0: /* Custom controls */
		DrawCustomCtrl(hDC, &rt, pCtrl->wndClass);
		break;
	case 1: /* Check boxes */
		DrawCRControl(hDC, &rt, pCtrl->text, DFCS_BUTTONCHECK);
		break;
	case 2: /* Radio buttons */
		DrawCRControl(hDC, &rt, pCtrl->text, DFCS_BUTTONRADIO);
		break;
	case 3: /* Client boxes */
		if (pCtrl->rendType == 3) /* ICON */
			DrawClientBox(hDC, &rt, TRUE);
		else
			DrawClientBox(hDC, &rt, FALSE);
		break;
	case 4: /* Draw text only */
	{
		UINT align;
		if (pCtrl->rendType == 0)
			align = DT_LEFT | DT_WORDBREAK;
		if (pCtrl->rendType == 1)
			align = DT_CENTER | DT_WORDBREAK;
		if (pCtrl->rendType == 2)
			align = DT_RIGHT | DT_WORDBREAK;
		if (pCtrl->rendType == 4)
			align = DT_CENTER | DT_VCENTER | DT_SINGLELINE;
		DrawDlgText(hDC, &rt, pCtrl->text, align);
		break;
	}
	case 5: /* Push buttons */
	{
		BOOL defBtn;
		if (pCtrl->rendType == 0)
			defBtn = TRUE;
		else defBtn = FALSE;
		DrawButton(hDC, &rt, pCtrl->text, defBtn);
		break;
	}
	case 6: /* Group box */
		if (pCtrl->style == NULL)
			break;
		if (strlen(pCtrl->style) == 0 ||
			strstr("BS_LEFT", pCtrl->style) != NULL)
			DrawGroupBox(hDC, &rt, pCtrl->text, DT_LEFT);
		else if (strstr("BS_CENTER", pCtrl->style) != NULL)
			DrawGroupBox(hDC, &rt, pCtrl->text, DT_CENTER);
		else if (strstr("BS_RIGHT", pCtrl->style) != NULL)
			DrawGroupBox(hDC, &rt, pCtrl->text, DT_RIGHT);
		break;
	case 7: /* Scroll controls */
		if (pCtrl->style == NULL)
			break;
		if (strstr("SBS_VERT", pCtrl->style) != NULL)
			DrawScrollbar(hDC, &rt, SB_VERT);
		if (strstr("SBS_HORZ", pCtrl->style) != NULL)
			DrawScrollbar(hDC, &rt, SB_HORZ);
		break;
	}

	if (activeCtrl == ctrlNum)
	{
		DlgUnitMap(&rt);
		DrawSelRect(hDC, &rt);
	}
}

/* Draws a check box or a radio button */
void DrawCRControl(HDC hDC, RECT* rt, char* caption, UINT state)
{
	RECT scRt;
	long smX, smY; /* Checkbox or radio button sizes */
	CopyRect(&scRt, rt);
	smX = 13; /* Where do these sizes come from? */
	smY = 13;
	/* Draw the button */
	DlgUnitMap(&scRt);
	scRt.top += max((fontHeight / 2) - smY / 2, 0); /* Vertical centering */
	scRt.right = scRt.left + smX;
	scRt.bottom = scRt.top + smY;
	DrawFrameControl(hDC, &scRt, DFC_BUTTON, state);
	/* Draw the text */
	scRt.left += smX + smX / 2;
	scRt.right = DLG2SCR_X(rt->right);
	scRt.top = DLG2SCR_Y(rt->top);
	/*if (fontHeight < smY)
		scRt.top += 8 - (fontHeight / 2);*/
	scRt.bottom = DLG2SCR_Y(rt->bottom);
	DrawText(hDC, caption, -1, &scRt, DT_LEFT);
}

void DrawButton(HDC hDC, RECT* rt, char* caption, BOOL defBtn)
{
	RECT scRt;
	/* Draw the button */
	CopyRect(&scRt, rt);
	DlgUnitMap(&scRt);
	if (defBtn == TRUE)
	{
		/* Draw a black outline */
		HBRUSH hBr;
		hBr = (HBRUSH)GetStockObject(NULL_BRUSH);
		SelectObject(hDC, hBr);
		Rectangle(hDC, scRt.left, scRt.top, scRt.right, scRt.bottom);
		InflateRect(&scRt, -1, -1); /* Draw button in outline */
	}
	DrawFrameControl(hDC, &scRt, DFC_BUTTON, DFCS_BUTTONPUSH);
	/* Draw the text */
	DrawText(hDC, caption, -1, &scRt, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
}

void DrawClientBox(HDC hDC, RECT* rt, BOOL icon)
{
	RECT scRt;
	HBRUSH hBr;
	CopyRect(&scRt, rt);
	DlgUnitMap(&scRt);
	if (icon == TRUE)
	{
		scRt.right = scRt.left + GetSystemMetrics(SM_CXICON);
		scRt.bottom = scRt.top + GetSystemMetrics(SM_CYICON);
	}
	hBr = GetSysColorBrush(COLOR_WINDOW);
	FillRect(hDC, &scRt, hBr);
	DrawEdge(hDC, &scRt, BDR_SUNKENINNER | BDR_SUNKENOUTER, BF_RECT);
}

void DrawDlgText(HDC hDC, RECT* rt, char* caption, UINT align)
{
	RECT scRt;
	CopyRect(&scRt, rt);
	DlgUnitMap(&scRt);
	DrawText(hDC, caption, -1, &scRt, align);
}

void DrawGroupBox(HDC hDC, RECT* rt, char* caption, UINT textAlign)
{
	RECT scRt;
	COLORREF oldBk;
	CopyRect(&scRt, rt);
	/* Move the top edge for the caption */
	scRt.top += 4;
	DlgUnitMap(&scRt);
	DrawEdge(hDC, &scRt, BDR_SUNKENOUTER | BDR_RAISEDINNER, BF_RECT);
	/* Draw the text */
	CopyRect(&scRt, rt);
	scRt.left += 4;
	scRt.bottom = scRt.top + 8;
	scRt.right -= 4;
	DlgUnitMap(&scRt);
	SetBkMode(hDC, OPAQUE);
	oldBk = GetBkColor(hDC);
	SetBkColor(hDC, GetSysColor(COLOR_3DFACE));
	DrawText(hDC, caption, -1, &scRt, textAlign | DT_SINGLELINE);
	SetBkColor(hDC, oldBk);
	SetBkMode(hDC, TRANSPARENT);
}

void DrawScrollbar(HDC hDC, RECT* rt, UINT type)
{
	RECT scRt;
	RECT subRt;
	HBRUSH hBr;
	CopyRect(&scRt, rt);
	DlgUnitMap(&scRt);
	if (type == SB_VERT)
	{
		/* Draw the shaded area */
		/* The shaded area is a checker pattern between COLOR_3DFACE
		   and COLOR_3DHIGHLIGHT, (perhaps COLOR_SCROLLBAR) */
		/* Right now, we won't draw the shaded area */
		CopyRect(&subRt, &scRt);
		hBr = GetSysColorBrush(COLOR_3DFACE);
		if ((scRt.bottom - scRt.top) >= GetSystemMetrics(SM_CYVSCROLL) * 2)
		{
			subRt.top += GetSystemMetrics(SM_CYVSCROLL);
			subRt.bottom -= GetSystemMetrics(SM_CYVSCROLL);
			FillRect(hDC, &subRt, hBr);
			DrawEdge(hDC, &subRt, BDR_RAISEDOUTER | BDR_RAISEDINNER, BF_RECT);
		}
		/* Draw the top arrow */
		if ((scRt.bottom - scRt.top) < GetSystemMetrics(SM_CYVSCROLL) * 2)
		{
			subRt.top = scRt.top;
			subRt.bottom = subRt.top + (scRt.bottom - scRt.top) / 2;
		}
		else
		{
			subRt.top -= GetSystemMetrics(SM_CYVSCROLL);
			subRt.bottom = subRt.top + GetSystemMetrics(SM_CYVSCROLL);
		}
		DrawFrameControl(hDC, &subRt, DFC_SCROLL, DFCS_SCROLLUP);
		/* Draw the bottom arrow */
		if ((scRt.bottom - scRt.top) < GetSystemMetrics(SM_CYVSCROLL) * 2)
		{
			subRt.top = subRt.bottom;
			subRt.bottom = scRt.bottom;
		}
		else
		{
			subRt.bottom = DLG2SCR_Y(rt->bottom);
			subRt.top = subRt.bottom - GetSystemMetrics(SM_CYVSCROLL);
		}
		DrawFrameControl(hDC, &subRt, DFC_SCROLL, DFCS_SCROLLDOWN);
	}
	if (type == SB_HORZ)
	{
		/* Draw the shaded area */
		/* The shaded area is a checker pattern between COLOR_3DFACE
		   and COLOR_3DHIGHLIGHT */
		/* Right now, we won't draw the shaded area */
		CopyRect(&subRt, &scRt);
		hBr = GetSysColorBrush(COLOR_3DFACE);
		if ((scRt.right - scRt.left) >= GetSystemMetrics(SM_CXHSCROLL) * 2)
		{
			subRt.left += GetSystemMetrics(SM_CXHSCROLL);
			subRt.right -= GetSystemMetrics(SM_CXHSCROLL);
			FillRect(hDC, &subRt, hBr);
			DrawEdge(hDC, &subRt, BDR_RAISEDOUTER | BDR_RAISEDINNER, BF_RECT);
		}
		/* Draw the left arrow */
		if ((scRt.right - scRt.left) < GetSystemMetrics(SM_CXHSCROLL) * 2)
		{
			subRt.left = scRt.left;
			subRt.right = subRt.left + (scRt.right - scRt.left) / 2;
		}
		else
		{
			subRt.left -= GetSystemMetrics(SM_CXHSCROLL);
			subRt.right = subRt.left + GetSystemMetrics(SM_CXHSCROLL);
		}
		DrawFrameControl(hDC, &subRt, DFC_SCROLL, DFCS_SCROLLLEFT);
		/* Draw the right arrow */
		if ((scRt.right - scRt.left) < GetSystemMetrics(SM_CXHSCROLL) * 2)
		{
			subRt.left = subRt.right;
			subRt.right = scRt.right;
		}
		else
		{
			subRt.right = DLG2SCR_X(rt->right);
			subRt.left = subRt.right - GetSystemMetrics(SM_CXHSCROLL);
		}
		DrawFrameControl(hDC, &subRt, DFC_SCROLL, DFCS_SCROLLRIGHT);
	}
}

void DrawCustomCtrl(HDC hDC, RECT* rt, char* clsName)
{
	RECT scRt;
	HBRUSH hBr;
	CopyRect(&scRt, rt);
	DlgUnitMap(&scRt);
	/* Draw the size of the control */
	hBr = GetSysColorBrush(COLOR_APPWORKSPACE);
	FillRect(hDC, &scRt, hBr);
	/* Draw the class name of the control */
	DrawText(hDC, clsName, -1, &scRt, DT_LEFT);
}

/* This is so that we do not need to create a pseudo dialog box for
   MapDialogRect() */
void DlgUnitMap(RECT* rt)
{
	rt->left = DLG2SCR_X(rt->left);
	rt->top = DLG2SCR_Y(rt->top);
	rt->right = DLG2SCR_X(rt->right);
	rt->bottom = DLG2SCR_Y(rt->bottom);
}
