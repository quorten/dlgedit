/* User-friendly file system code.

   Contains standard provisions for asking the user if they want to
   save their file before closing and marking if the file was
   modified.  Perhaps in the future it will contain an undo and redo
   module. */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>

/* We need g_hInstance and SaveDialogTemplate() */
#include "dlgedit.h"

char curFilename[MAX_PATH];
BOOL noFileLoaded;
BOOL fileChanged;

/* Shows a dialog to open a file if "type" is zero, or shows a
   dialog to save the current file if "type" is one. */
BOOL GetFilenameDialog(char* filename, char* dlgTitle, HWND hwnd,
					   unsigned type)
{
	OPENFILENAME ofn;
	char* filterStr = "Dialog Templates (*.dlg)\0*.dlg\0All Files\0*.*\0";

	ZeroMemory(&ofn, sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hwnd;
	ofn.hInstance = g_hInstance;
	ofn.lpstrFile = filename;
	ofn.nMaxFile = MAX_PATH;
	if (type == 0) /* Open */
		ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
	else
		ofn.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
	ofn.lpstrFilter = filterStr;
	ofn.lpstrDefExt = "dlg";
	ofn.lpstrTitle = dlgTitle;

	switch (type)
	{
	case 0: /* Open */
		return GetOpenFileName(&ofn);
	case 1: /* Save */
		return GetSaveFileName(&ofn);
	}
	return FALSE;
}

/* Asks the user if they want to save the file if the file was
   modified.  Returns zero if the application should not continue the
   requested operation or one if the file was successfully saved or
   not. */
int AskForSave(HWND hwnd)
{
	if (fileChanged == TRUE)
	{
		int result;
		result = MessageBox(hwnd,
			"Do you want to save your changes to the current file?",
			"Confirm", MB_YESNOCANCEL | MB_ICONEXCLAMATION);
		switch (result)
		{
		case IDYES:
			if (noFileLoaded == TRUE)
			{
				if (GetFilenameDialog(curFilename, "Save As", hwnd, 1))
				{
					if (!SaveDialogTemplate(curFilename))
					{
						MessageBox(hwnd,
								   "There was an error saving the file.",
								   "Error", MB_OK | MB_ICONEXCLAMATION);
						return 0;
					}
				}
				else
					return 0;
			}
			else if (!SaveDialogTemplate(curFilename))
			{
				MessageBox(hwnd,
						   "There was an error saving the file.",
						   "Error", MB_OK | MB_ICONEXCLAMATION);
				return 0;
			}
			break;
		case IDNO:
			/* Do nothing */
			break;
		case IDCANCEL:
			return 0; /* Don't destroy window */
		}
	}
	return 1;
}
