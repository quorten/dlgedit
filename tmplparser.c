/* Dialog template parsing and generation code
   Also contains code for the dialog editor data model.

   This is platform independent code.

   Note on the data model: if a string variable has a limit of 255
   characters, that limit is intentionally enforced! */

#include <stdio.h>
#include <string.h>

#include "xmalloc.h"
#include "tmplparser.h"

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

/* Drawn controls are grouped by drawing style */
const unsigned numClasses = 8;
const unsigned numEachClass[] = {1, 4, 2, 4, 4, 2, 1, 1};
const unsigned maxPerClass = 4;
const char* drawClasses[8][4] = {
	{"CONTROL"}, /* Custom controls */
	{"AUTO3STATE", "STATE3", "AUTOCHECKBOX", "CHECKBOX"}, /* Check boxes */
	{"AUTORADIOBUTTON", "RADIOBUTTON"}, /* Radio buttons */
	{"EDITTEXT", "LISTBOX", "COMBOBOX", "ICON"}, /* Client boxes */
	{"LTEXT", "CTEXT", "RTEXT", "PUSHBOX"}, /* Draw text only */
	{"DEFPUSHBUTTON", "PUSHBUTTON"}, /* Push buttons */
	{"GROUPBOX"}, /* Group box */
	{"SCROLLBAR"}}; /* Scroll controls */

/* Dialog variables */
char* dlgHead;
POINT dlgPos;
long dlgWidth;
long dlgHeight;
BOOL dlgHasCaption;
char dlgCaption[256];
unsigned dlgPointSize;
char* dlgFontFam;
DlgItem_array dlgControls;

/* Parser variables */
unsigned curPos;
unsigned curLine;
char* errorDesc;

/* Translates a text buffer to Unix line endings in place.  Returns
   the new size of the data, which may have shrunken.

   "dataSize" specifies the length of the string, not including the
   null character. */
unsigned SetUnixNlChars(char* buffer, unsigned dataSize)
{
	unsigned i, j;
	for (i = 0, j = 0; i < dataSize; i++)
	{
		if (buffer[i] != '\r')
			buffer[j] = buffer[i];
		else
		{
			if (buffer[i+1] == '\n') /* CR+LF */
				i++;
			buffer[j] = '\n';
		}
		j++;
	}
	buffer[j] = '\0';
	return j;
}

/* The caller of this function MUST free the returned memory.

   "dataSize" specifies the length of the string, not including the
   null character. */
heap_char GenWinNlChars(char* buffer, unsigned dataSize)
{
	char* winBuff;
	unsigned i, j;
	winBuff = (char*)xmalloc(dataSize + 1);
	for (i = 0, j = 0; i < dataSize; i++)
	{
		if (buffer[i] == '\n')
			winBuff[j++] = '\r';
		if (j >= dataSize)
			winBuff = (char*)xrealloc(winBuff, j + 2);
		winBuff[j++] = buffer[i];
		if (j >= dataSize)
			winBuff = (char*)xrealloc(winBuff, j + 2);
	}
	winBuff[j] = '\0';
	return winBuff; /* This MUST be freed by the caller */
}

/* Translates escape codes in a string to characters.  Returns the new
   data size.  This is not a fully featured escape code translator.

   "dataSize" specifies the length of the string, not including the
   null character. */
unsigned TransEscapeChars(char* buffer, unsigned dataSize)
{
	unsigned curPos;
	unsigned newDataSize;
	curPos = 0;
	newDataSize = dataSize;
	while (curPos < newDataSize)
	{
		if (buffer[curPos] == '\\')
		{
			BOOL foundEscChar;
			foundEscChar = TRUE;
			curPos++;
			if (curPos >= newDataSize)
				break;
			switch (buffer[curPos])
			{
			case '\\': break;
			case 'a': buffer[curPos] = '\a'; break;
			case 'b': buffer[curPos] = '\b'; break;
			case 'f': buffer[curPos] = '\f'; break;
			case 'n': buffer[curPos] = '\n'; break;
			case 'r': buffer[curPos] = '\r'; break;
			case 'v': buffer[curPos] = '\v'; break;
			case 't': buffer[curPos] = '\t'; break;
			case '\'': break;
			case '\"': break;
			case '?': break;
			default:
				foundEscChar = FALSE;
				break;
			}
			if (foundEscChar == TRUE)
			{
				memmove(&buffer[curPos-1], &buffer[curPos],
					newDataSize - curPos);
				curPos--;
				newDataSize--;
			}
		}
		curPos++;
	}
	buffer[newDataSize] = '\0';
	return newDataSize;
}

/* Reallocates the dynamically allocated memory pointed to by "buffer"
   and writes a string with special characters replaced by escape
   codes.  Returns the new buffer pointer.

   "dataSize" specifies the length of the string, not including the
   null character. */
char* UntransEscChars(char* buffer, unsigned dataSize)
{
	char* newBuffer;
	unsigned curPos;
	unsigned newDataSize;
	newBuffer = buffer;
	curPos = 0;
	newDataSize = dataSize;
	while (curPos < newDataSize)
	{
		BOOL foundEscChar;
		char c;
		foundEscChar = TRUE;
		switch (newBuffer[curPos])
		{
		case '\\': c = '\\'; break;
		case '\a': c = 'a'; break;
		case '\b': c = 'b'; break;
		case '\f': c = 'f'; break;
		case '\n': c = 'n'; break;
		case '\r': c = 'r'; break;
		case '\v': c = 'v'; break;
		case '\t': c = 't'; break;
		case '\'': c = '\''; break;
		case '\"': c = '\"'; break;
		default:
			foundEscChar = FALSE;
			break;
		}
		if (foundEscChar == TRUE)
		{
			newBuffer[curPos] = '\\';
			curPos++;
			newDataSize++;
			newBuffer = (char*)xrealloc(newBuffer, newDataSize + 1);
			memmove(&newBuffer[curPos+1], &newBuffer[curPos],
					(newDataSize - 1) - curPos);
			newBuffer[curPos] = c;
		}
		curPos++;
	}
	newBuffer[newDataSize] = '\0';
	return newBuffer;
}

#define CHECK_SIZE_ERROR(errLabel) \
	if (curPos >= dataSize) \
		goto errLabel;
#define WHITESPACE (buffer[curPos] == ' ' || buffer[curPos] == '\t')
#define CHECK_CHAR(chcode) (buffer[curPos] == chcode)
#define SKIP_CHAR(chcode) \
	if (!CHECK_CHAR(chcode)) \
		goto headError; \
	curPos++;
#define SAVE_STRING() \
	lastString = (char*)xrealloc(lastString, curPos - lastPos + 1); \
	strncpy(lastString, &buffer[lastPos], curPos - lastPos); \
	lastString[curPos-lastPos] = '\0';

#define WS_AND_NL (WHITESPACE || buffer[curPos] == '\n')
#define SKIP_WHITESPACE() \
	while (curPos < dataSize && WHITESPACE) \
		curPos++;
#define CHECK_NO_NL_ERROR(label) \
	if (CHECK_CHAR('\n')) \
		goto label;

/* This function stores dynamically allocated memory.  Therefore, you
   must make sure that you free the relevent memory before calling
   this function for recalculations.

   "dataSize" specifies the length of the string, not including the
   null character. */
BOOL ParseDlgHead(char* buffer, unsigned dataSize)
{
	unsigned lastPos;
	char* lastString;
	BOOL foundHeadEnd;
	/* Temporary variables */
	unsigned i;
	lastPos = 0;
	foundHeadEnd = FALSE;

	curPos = 0;
	curLine = 1;
	dlgHead = NULL;
	dlgFontFam = NULL;

	lastString = (char*)xmalloc(9);

	/* Always skip comments (not done right now) */

	/* Skip whitespace and newlines */
	while (curPos < dataSize && WS_AND_NL)
	{
		if (CHECK_CHAR('\n'))
			curLine++;
		curPos++;
	}
	errorDesc = "Missing dialog template data.";
	CHECK_SIZE_ERROR(headError);
	CHECK_NO_NL_ERROR(headError);

	/* Skip ID */
	lastPos = curPos;
	while (curPos < dataSize && !WS_AND_NL)
		curPos++;
	errorDesc = "Missing dialog ID.";
	CHECK_SIZE_ERROR(headError);
	CHECK_NO_NL_ERROR(headError);

	errorDesc = "Missing space after dialog ID.";
	SKIP_WHITESPACE();
	CHECK_SIZE_ERROR(headError);
	CHECK_NO_NL_ERROR(headError);

	/* Skip DIALOG or DIALOGEX */
	lastPos = curPos;
	while (curPos < dataSize && !WS_AND_NL)
		curPos++;
	errorDesc = "Missing dialog resource specifier.";
	CHECK_SIZE_ERROR(headError);
	CHECK_NO_NL_ERROR(headError);

	errorDesc = "Missing space after DIALOG or DIALOGEX.";
	SKIP_WHITESPACE();
	CHECK_SIZE_ERROR(headError);
	CHECK_NO_NL_ERROR(headError);

	/* Read x, y, w, h */
	for (i = 0; i < 4; i++)
	{
		lastPos = curPos;
		while (curPos < dataSize && !CHECK_CHAR(',') && !WHITESPACE &&
			!CHECK_CHAR('\n'))
			curPos++;
		if (i < 3)
			errorDesc = "Missing dialog coordinates.";
		else
			errorDesc = "Missing dialog template data.";
		CHECK_SIZE_ERROR(headError);
		SAVE_STRING();
		/* Data processing hook */
		{
			int numVal;
			numVal = atoi(lastString);
			switch (i)
			{
			case 0: dlgPos.x = numVal; break;
			case 1: dlgPos.y = numVal; break;
			case 2: dlgWidth = numVal; break;
			case 3: dlgHeight = numVal; break;
			}
		}
		if (i < 3)
			curPos++; /* Skip the comma */
		SKIP_WHITESPACE();
		CHECK_SIZE_ERROR(headError);
		if (i < 3)
			CHECK_NO_NL_ERROR(headError);
	}
	errorDesc = "Expected end of line.";
	SKIP_CHAR('\n');
	curLine++;
	errorDesc = "Missing dialog template data.";
	CHECK_SIZE_ERROR(headError);

	/* Read only a CAPTION or a FONT statement */
	/* Otherwise, skip until "BEGIN" or '{' */
	dlgHasCaption = FALSE;
	errorDesc = "Missing beginning marker of dialog control list.";
	while (curPos < dataSize && foundHeadEnd == FALSE)
	{
		SKIP_WHITESPACE();
		CHECK_SIZE_ERROR(headError);
		if (strncmp("CAPTION", &buffer[curPos], 7) == 0)
		{
			curPos += 7;
			errorDesc = "Missing dialog caption string.";
			CHECK_SIZE_ERROR(headError);
			SKIP_WHITESPACE();
			CHECK_SIZE_ERROR(headError);
			SKIP_CHAR('"');
			CHECK_SIZE_ERROR(headError);
			lastPos = curPos;
			while (curPos < dataSize && !CHECK_CHAR('"'))
			{
				if (CHECK_CHAR('\\'))
				{
					/* Ignore escaped quotation marks */
					curPos++;
					CHECK_SIZE_ERROR(headError);
				}
				curPos++;
			}
			errorDesc = "Missing closing quote on dialog caption string.";
			CHECK_SIZE_ERROR(headError);
			/* Skip the quote after saving the string so not to include it */
			SAVE_STRING();
			SKIP_CHAR('"');
			/* Data processing hook */
			{
				unsigned maxCpy;
				maxCpy = strlen(lastString);
				if (maxCpy > 255)
					maxCpy = 255;
				strncpy(dlgCaption, lastString, maxCpy);
				dlgCaption[maxCpy] = '\0';
				dlgHasCaption = TRUE;
			}
			/* Reset the error description */
			errorDesc = "Missing beginning marker of dialog control list.";
		}
		else if (strncmp("FONT", &buffer[curPos], 4) == 0)
		{
			curPos += 4;
			errorDesc = "Missing font point size.";
			CHECK_SIZE_ERROR(headError);
			SKIP_WHITESPACE();
			/* Read the point size */
			lastPos = curPos;
			while (curPos < dataSize && !CHECK_CHAR(','))
				curPos++;
			CHECK_SIZE_ERROR(headError);
			SAVE_STRING();
			/* Data processing hook */
			dlgPointSize = atoi(lastString);
			errorDesc = "Missing font face name.";
			curPos++; /* Skip the comma */
			CHECK_SIZE_ERROR(headError);
			SKIP_WHITESPACE();
			CHECK_SIZE_ERROR(headError);
			/* Read the font family */
			CHECK_SIZE_ERROR(headError);
			SKIP_CHAR('"');
			CHECK_SIZE_ERROR(headError);
			lastPos = curPos;
			while (curPos < dataSize && !CHECK_CHAR('"'))
			{
				if (CHECK_CHAR('\\'))
				{
					/* Ignore escaped quotation marks */
					curPos++;
					CHECK_SIZE_ERROR(headError);
				}
				curPos++;
			}
			errorDesc = "Missing closing quote on font face string.";
			CHECK_SIZE_ERROR(headError);
			/* Skip the quote after saving the string so not to include it */
			SAVE_STRING();
			SKIP_CHAR('"');
			/* Data processing hook */
			dlgFontFam = (char*)xmalloc(strlen(lastString) + 1);
			strcpy(dlgFontFam, lastString);
			/* Reset the error description */
			errorDesc = "Missing beginning marker of dialog control list.";
		}
		else if (CHECK_CHAR('{') ||
			strncmp("BEGIN", &buffer[curPos], 5) == 0)
		{
			/* Quit at end of line */
			foundHeadEnd = TRUE;
			/* Read backwards so not to include the BEGIN line *
			while (curPos > 0 && !CHECK_CHAR('\n'))
				curPos--;
			if (curPos == 0)
				goto headError;
			curPos++; /* Include the newline character *
			headEndPos = curPos;
			/* Continue to skip this line */
		}
		while (curPos < dataSize && !CHECK_CHAR('\n'))
			curPos++;
		CHECK_SIZE_ERROR(headError);
		SKIP_CHAR('\n');
		curLine++;
		CHECK_SIZE_ERROR(headError);
	}
	goto noHeadError;
headError:
	xfree(lastString);
	return FALSE;
noHeadError:
	/* Save the header */
	lastPos = 0;
	SAVE_STRING();
	dlgHead = (char*)xmalloc(strlen(lastString) + 1);
	strcpy(dlgHead, lastString);
	xfree(lastString);
	return TRUE;
}

/* If you call this function on a buffer that has a single control in
   it and no dialog header, you will have to set "curPos" to zero
   before calling this function.

   This function also stores dynamically allocated memory.  Therefore,
   you must make sure that you free the relevent memory before calling
   this function for recalculations.

   "dataSize" specifies the length of the string, not including the
   null character. */
BOOL ParseControl(char* buffer, unsigned dataSize, unsigned ctrlNum)
{
	unsigned i, j;
	BOOL foundType;
	unsigned lastPos;
	char* lastString;
	DlgItem* pCtrl;

	pCtrl = &dlgControls.d[ctrlNum];
	pCtrl->id = NULL;
	pCtrl->style = NULL;
	pCtrl->exStyle = NULL;

	foundType = FALSE;
	lastString = (char*)xmalloc(9);

	/* Assume "curPos" is directed to the beginning of the proper line */
	SKIP_WHITESPACE();
	errorDesc = "Missing control type statement.";
	CHECK_SIZE_ERROR(ctrlError);
	/* Check the type of control */
	for (i = 0; i < numClasses; i++)
	{
		for (j = 0; j < numEachClass[i]; j++)
		{
			if (strncmp(drawClasses[i][j], &buffer[curPos],
				strlen(drawClasses[i][j])) == 0)
			{
				/* We have a match */
				pCtrl->rendClass = i;
				pCtrl->rendType = j;
				curPos += strlen(drawClasses[i][j]);
				errorDesc = "Missing control parameters.";
				CHECK_SIZE_ERROR(ctrlError);
				CHECK_NO_NL_ERROR(ctrlError);
				SKIP_WHITESPACE();
				CHECK_SIZE_ERROR(ctrlError);
				CHECK_NO_NL_ERROR(ctrlError);
				foundType = TRUE;
				break;
			}
		}
		if (foundType == TRUE)
			break;
	}
	if (foundType == FALSE)
	{
		errorDesc = "Unrecognized control type.";
		goto ctrlError;
	}

	/* Read the caption */
	/* Should SCROLLBAR really allow text? For now, no. */
	/* Control types without text:
	   COMBOBOX, EDITTEXT, LISTBOX, SCROLLBAR */
	if ((pCtrl->rendClass == 3 && /* Client boxes */
		pCtrl->rendType != 3) || /* ICON */
		pCtrl->rendClass == 7) /* Scrollbar */
		goto readID;

	if (pCtrl->rendClass == 3 && pCtrl->rendType == 3) /* ICON */
	{
		/* For ICON, "text" is actually the resource name and may not
		   be a string */
		/* Don't require quotes on the text entry */
		unsigned maxCpy;

		lastPos = curPos;
		while (curPos < dataSize && !CHECK_CHAR('\n') && !CHECK_CHAR(','))
			curPos++;
		errorDesc = "Missing control parameters after icon resource ID.";
		CHECK_SIZE_ERROR(ctrlError);
		CHECK_NO_NL_ERROR(ctrlError);
		SAVE_STRING();

		/* Data processing hook */
		maxCpy = curPos - lastPos;
		if (maxCpy > 255)
			maxCpy = 255;
		strncpy(pCtrl->text, lastString, maxCpy);
		pCtrl->text[maxCpy] = '\0';

		curPos++; /* Skip the comma */
		CHECK_SIZE_ERROR(ctrlError);
		CHECK_NO_NL_ERROR(ctrlError);
		SKIP_WHITESPACE();
		CHECK_SIZE_ERROR(ctrlError);
		CHECK_NO_NL_ERROR(ctrlError);

		goto readID;
	}

	errorDesc = "Missing quotes around caption text.";
	if (!CHECK_CHAR('"'))
		goto ctrlError;
	curPos++; /* Skip the quote */
	lastPos = curPos;
	while (curPos < dataSize && !CHECK_CHAR('\n') && !CHECK_CHAR('"'))
	{
		if (CHECK_CHAR('\\'))
		{
			/* Ignore escaped quotation marks */
			curPos++;
			CHECK_SIZE_ERROR(ctrlError);
			CHECK_NO_NL_ERROR(ctrlError);
		}
		curPos++;
	}
	CHECK_SIZE_ERROR(ctrlError);
	CHECK_NO_NL_ERROR(ctrlError);

	/* Skip the quote after saving the string so not to include it */
	SAVE_STRING();
	if (!CHECK_CHAR('"'))
		goto ctrlError;
	curPos++; /* Skip the quote */
	errorDesc = "Missing control parameters after caption parameter.";
	CHECK_SIZE_ERROR(ctrlError);
	CHECK_NO_NL_ERROR(ctrlError);

	/* Data processing hook */
	{
		unsigned maxCpy;
		maxCpy = TransEscapeChars(lastString, strlen(lastString));
		if (maxCpy > 255)
			maxCpy = 255;
		strncpy(pCtrl->text, lastString, maxCpy);
		pCtrl->text[maxCpy] = '\0';
	}

	curPos++; /* Skip the comma */
	CHECK_SIZE_ERROR(ctrlError);
	CHECK_NO_NL_ERROR(ctrlError);
	SKIP_WHITESPACE();
	CHECK_SIZE_ERROR(ctrlError);
	CHECK_NO_NL_ERROR(ctrlError);

readID: /* Read the ID */
	lastPos = curPos;
	while (curPos < dataSize && !CHECK_CHAR('\n') && !CHECK_CHAR(','))
		curPos++;
	errorDesc = "Missing control parameters after ID parameter.";
	CHECK_SIZE_ERROR(ctrlError);
	CHECK_NO_NL_ERROR(ctrlError);
	SAVE_STRING();

	/* Data processing hook */
	pCtrl->id = (char*)xmalloc(curPos - lastPos + 1);
	strcpy(pCtrl->id, lastString);

	curPos++; /* Skip the comma */
	CHECK_SIZE_ERROR(ctrlError);
	CHECK_NO_NL_ERROR(ctrlError);
	SKIP_WHITESPACE();
	CHECK_SIZE_ERROR(ctrlError);
	CHECK_NO_NL_ERROR(ctrlError);

	if (pCtrl->rendClass == 0) /* CONTROL */
	{
		/* Read the class */
		lastPos = curPos;
		while (curPos < dataSize && !CHECK_CHAR('\n') && !CHECK_CHAR(','))
			curPos++;
		errorDesc = "Missing control parameters after class parameter.";
		CHECK_SIZE_ERROR(ctrlError);
		CHECK_NO_NL_ERROR(ctrlError);
		SAVE_STRING();

		/* Data processing hook */
		{
			unsigned maxCpy;
			maxCpy = curPos - lastPos;
			if (maxCpy > 255)
				maxCpy = 255;
			strncpy(pCtrl->wndClass, lastString, maxCpy);
			pCtrl->wndClass[maxCpy] = '\0';
		}

		curPos++; /* Skip the comma */
		CHECK_SIZE_ERROR(ctrlError);
		CHECK_NO_NL_ERROR(ctrlError);
		SKIP_WHITESPACE();
		CHECK_SIZE_ERROR(ctrlError);
		CHECK_NO_NL_ERROR(ctrlError);

		/* Read the style */
		lastPos = curPos;
		while (curPos < dataSize && !CHECK_CHAR('\n') && !CHECK_CHAR(','))
			curPos++;
		errorDesc = "Missing control parameters after style parameter.";
		CHECK_SIZE_ERROR(ctrlError);
		CHECK_NO_NL_ERROR(ctrlError);
		SAVE_STRING();

		/* Data processing hook */
		pCtrl->style = (char*)xmalloc(curPos - lastPos + 1);
		strcpy(pCtrl->style, lastString);

		curPos++; /* Skip the comma */
		CHECK_SIZE_ERROR(ctrlError);
		CHECK_NO_NL_ERROR(ctrlError);
		SKIP_WHITESPACE();
		CHECK_SIZE_ERROR(ctrlError);
		CHECK_NO_NL_ERROR(ctrlError);
	}

	/* Read x, y, w, h */
	for (i = 0; i < 4; i++)
	{
		lastPos = curPos;
		while (curPos < dataSize && buffer[curPos] != ',' && !WHITESPACE &&
			!CHECK_CHAR('\n'))
			curPos++;
		if (i < 3)
			errorDesc = "Missing control dimensions.";
		else
			errorDesc = "Missing newline character.";
		CHECK_SIZE_ERROR(ctrlError);
		if (i < 3) CHECK_NO_NL_ERROR(ctrlError);
		SAVE_STRING();
		/* Data processing hook */
		{
			int numVal;
			numVal = atoi(lastString);
			switch (i)
			{
			case 0: pCtrl->x = numVal; break;
			case 1: pCtrl->y = numVal; break;
			case 2: pCtrl->cx = numVal; break;
			case 3: pCtrl->cy = numVal; break;
			}
		}
		if (CHECK_CHAR(','))
			curPos++;
		SKIP_WHITESPACE();
		CHECK_SIZE_ERROR(ctrlError);
		if (i < 3) CHECK_NO_NL_ERROR(ctrlError);
	}

	/* Read the style and extended style */
	/* Just save the entire line ending */
	lastPos = curPos;
	while (curPos < dataSize && !CHECK_CHAR('\n'))
		curPos++;
	CHECK_SIZE_ERROR(ctrlError);
	SAVE_STRING();
	if (pCtrl->rendClass == 0)
	{
		/* Save in exStyle */
		pCtrl->exStyle = (char*)xmalloc(curPos - lastPos + 1);
		strcpy(pCtrl->exStyle, lastString);
	}
	else
	{
		/* Save in style */
		pCtrl->style = (char*)xmalloc(curPos - lastPos + 1);
		strcpy(pCtrl->style, lastString);
		pCtrl->exStyle = NULL;
	}
	errorDesc = "Missing newline character.";
	curPos++; /* Skip the newline character */
	curLine++;
	CHECK_SIZE_ERROR(ctrlError);
	goto noCtrlError;
ctrlError:
	xfree(lastString);
	return FALSE;
noCtrlError:
	xfree(lastString);
	return TRUE;
}

/* All we really do for this function is parse and update the relevant
   fields. */
void FmtDlgHeader()
{
	char* buffer;
	unsigned dataSize;
	unsigned lastPos;
	buffer = dlgHead;
	dataSize = strlen(dlgHead);
	curPos = 0;
	lastPos = 0;
	/* Skip whitespace and newlines */
	while (curPos < dataSize && WS_AND_NL)
	{
		if (CHECK_CHAR('\n'))
			curLine++;
		curPos++;
	}
	CHECK_SIZE_ERROR(fmtHInternalErr);
	if (CHECK_CHAR('\n'))
		goto fmtHInternalErr;

	/* Skip ID */
	lastPos = curPos;
	while (curPos < dataSize && !WS_AND_NL)
		curPos++;
	CHECK_SIZE_ERROR(fmtHInternalErr);
	if (CHECK_CHAR('\n'))
		goto fmtHInternalErr;

	SKIP_WHITESPACE();
	CHECK_SIZE_ERROR(fmtHInternalErr);
	if (CHECK_CHAR('\n'))
		goto fmtHInternalErr;

	/* Skip DIALOG or DIALOGEX */
	lastPos = curPos;
	while (curPos < dataSize && !WS_AND_NL)
		curPos++;
	CHECK_SIZE_ERROR(fmtHInternalErr);
	if (CHECK_CHAR('\n'))
		goto fmtHInternalErr;

	SKIP_WHITESPACE();
	CHECK_SIZE_ERROR(fmtHInternalErr);
	if (CHECK_CHAR('\n'))
		goto fmtHInternalErr;

	/* Delete the old coordinates */
	lastPos = curPos;
	while (curPos < dataSize && !CHECK_CHAR('\n'))
		curPos++;
	CHECK_SIZE_ERROR(fmtHInternalErr);
	/* (dataSize + 1) Move the null character too */
	memmove(&dlgHead[lastPos], &dlgHead[curPos], (dataSize + 1) - curPos);
	dataSize -= curPos - lastPos;
	curPos = lastPos;

	/* Write the updated ones */
	{
		char* outString;
		/* Per programming convention, data (as opposed to single
		   variables) are always stored on the heap. */
		/* Maximum buffer size assuming 32-bit integers */
		outString = (char*)xmalloc(11 * 4 + 2 * 4 + 1);
		sprintf(outString, "%li, %li, %li, %li",
			dlgPos.x, dlgPos.y, dlgWidth, dlgHeight);

		dlgHead = (char*)xrealloc(dlgHead, dataSize + strlen(outString) + 1);
		memmove(&dlgHead[curPos+strlen(outString)], &dlgHead[curPos],
			(strlen(dlgHead) + 1) - curPos); /* Move the null character too */
		strncpy(&dlgHead[curPos], outString, strlen(outString));
		dataSize += strlen(outString);
		xfree(outString);
	}

	/* TODO: Update the font, need to store font family for this to work. */
fmtHInternalErr: ; /* This should never happen, but the label is here
					  in case it ever does. */
}

#define WRITE_DLG_VAR_TEXT(varname) \
	line = (char*)xrealloc(line, lineLen + strlen(pCtrl->varname) + 1); \
	strcpy(&line[lineLen], pCtrl->varname); \
	lineLen += strlen(pCtrl->varname);

#define WRITE_COMMA() \
	line = (char*)xrealloc(line, lineLen + 3); \
	strcpy(&line[lineLen], ", "); \
	lineLen += 2;

/* The caller of this function MUST xfree the returned memory. */
heap_char FmtControlText(unsigned ctrlNum)
{
	char* line;
	unsigned lineLen;
	DlgItem* pCtrl;
	/* Temporary variables */
	unsigned i, j;

	lineLen = 0;
	pCtrl = &dlgControls.d[ctrlNum];
	/* Write an initial tab */
	line = (char*)xmalloc(2);
	strcpy(&line[lineLen], "\t");
	lineLen += 1;

	/* Write the control name */
	for (i = 0; i < numClasses; i++)
	{
		for (j = 0; j < numEachClass[i]; j++)
		{
			if (pCtrl->rendClass == i && pCtrl->rendType == j)
			{
				line = (char*)xrealloc(line, lineLen +
					strlen(drawClasses[i][j]) + 1);
				strcpy(&line[lineLen], drawClasses[i][j]);
				lineLen += strlen(drawClasses[i][j]);
				break;
			}
		}
	}

	/* Write a space */
	line = (char*)xrealloc(line, lineLen + 2);
	strcpy(&line[lineLen], " ");
	lineLen += 1;

	/* Write the caption */
	/* Should SCROLLBAR really allow text? For now, no. */
	/* Control types without text:
	   COMBOBOX, EDITTEXT, LISTBOX, SCROLLBAR */
	if (pCtrl->rendClass == 3 && pCtrl->rendType == 3) /* ICON */
	{
		/* Don't write quotes for this part of the ICON control
		   statement, because it is a resource identifier. */
		WRITE_DLG_VAR_TEXT(text);
		WRITE_COMMA();
	}
	else if ((pCtrl->rendClass != 3 || /* Client boxes */
		pCtrl->rendType == 3) && /* ICON */
		pCtrl->rendClass != 7) /* Scrollbar */
	{
		char* untransLine;
		unsigned oldLen;
		unsigned untrLen;
		/* Write a quote */
		line = (char*)xrealloc(line, lineLen + 2);
		strcpy(&line[lineLen], "\"");
		lineLen += 1;

		/* Translate special characters to escape codes */
		oldLen = strlen(pCtrl->text);
		untransLine = (char*)xmalloc(oldLen + 1);
		strcpy(untransLine, pCtrl->text);
		untransLine = UntransEscChars(untransLine, oldLen);
		untrLen = strlen(untransLine);
		line = (char*)xrealloc(line, lineLen + untrLen + 1);
		strcpy(&line[lineLen], untransLine);
		lineLen += untrLen;
		xfree(untransLine);
		/* WRITE_DLG_VAR_TEXT(text); */

		/* Write a quote */
		line = (char*)xrealloc(line, lineLen + 2);
		strcpy(&line[lineLen], "\"");
		lineLen += 1;
		WRITE_COMMA();
	}

	/* Write the ID */
	WRITE_DLG_VAR_TEXT(id);
	WRITE_COMMA();

	if (pCtrl->rendClass == 0) /* CONTROL */
	{
		/* Write the class */
		WRITE_DLG_VAR_TEXT(wndClass);
		WRITE_COMMA();

		/* Write the style */
		WRITE_DLG_VAR_TEXT(style);
		WRITE_COMMA();
	}

	/* Write x, y, w, h */
	{
		char* outString;
		/* Per programming convention, data (as opposed to single
		   variables) are always stored on the heap. */
		/* Maximum buffer size assuming 32-bit integers */
		outString = (char*)xmalloc(11 * 4 + 2 * 4 + 1);
		sprintf(outString, "%i, %i, %i, %i",
			pCtrl->x, pCtrl->y, pCtrl->cx, pCtrl->cy);

		line = (char*)xrealloc(line, lineLen + strlen(outString) + 1);
		strcpy(&line[lineLen], outString);
		lineLen += strlen(outString);
		xfree(outString);
	}

	/* Write the style and extended style */
	if (pCtrl->rendClass == 0)
	{
		if (strlen(pCtrl->exStyle) != 0)
		{
			WRITE_COMMA();
			/* Write exStyle */
			WRITE_DLG_VAR_TEXT(exStyle);
		}
	}
	else if (strlen(pCtrl->style) != 0)
	{
		WRITE_COMMA();
		/* Just write style, it contains exStyle too */
		WRITE_DLG_VAR_TEXT(style);
	}

	/* Write a newline character */
	line = (char*)xrealloc(line, lineLen + 2);
	strcpy(&line[lineLen], "\n");
	lineLen += 1;

	return line; /* This MUST be freed by the caller */
}

void FreeDlgData()
{
	unsigned i;
	for (i = 0; i < dlgControls.len; i++)
	{
		xfree(dlgControls.d[i].id);
		xfree(dlgControls.d[i].style);
		xfree(dlgControls.d[i].exStyle);
	}
	xfree(dlgControls.d);
	dlgControls.d = NULL;
	dlgControls.len = 0;
	xfree(dlgFontFam);
	dlgFontFam = NULL;
	xfree(dlgHead);
	dlgHead = NULL;
}
