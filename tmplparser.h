/* Dialog template parsing and generation code
   Also contains code for the dialog editor data model. */

#ifndef TMPLPARSER_H
#define TMPLPARSER_H

#include "xmalloc.h"
#define ea_malloc xmalloc
#define ea_realloc xrealloc
#define ea_free xfree
#include "exparray.h"
/* Include Windows data types (BOOL and POINT) */
#include "subwindef.h"

/* We define our own structures for the purpose of saving preprocessor
   symbols in the dialog template. */
struct DlgItem_t
{
	/*char* itemType;*/
	char text[256];
	char* id;
	int x;
	int y;
	int cx;
	int cy;
	char* style;
	char* exStyle;
	char wndClass[256];
	int rendClass; /* Special rendering for recognized types */
	int rendType;
};

typedef struct DlgItem_t DlgItem;

EA_TYPE(DlgItem);

typedef char* heap_char; /* Designates data that must passed to free() */

unsigned SetUnixNlChars(char* buffer, unsigned dataSize);
heap_char GenWinNlChars(char* buffer, unsigned dataSize);
unsigned TransEscapeChars(char* buffer, unsigned dataSize);
char* UntransEscChars(char* buffer, unsigned dataSize);
BOOL ParseDlgHead(char* buffer, unsigned dataSize);
BOOL ParseControl(char* buffer, unsigned dataSize, unsigned ctrlNum);
void FmtDlgHeader();
heap_char FmtControlText(unsigned ctrlNum);
void FreeDlgData();

/* Dialog variables */
extern char* dlgHead;
extern POINT dlgPos;
extern long dlgWidth;
extern long dlgHeight;
extern BOOL dlgHasCaption;
extern char dlgCaption[256];
extern unsigned dlgPointSize;
extern char* dlgFontFam;
extern DlgItem_array dlgControls;

/* Parser variables */
extern unsigned curPos;
extern unsigned curLine;
extern char* errorDesc;

#endif
