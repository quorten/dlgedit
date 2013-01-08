/* Defines dynamic memory functions that do a few more saftey checks.
   You might decide to use "gnulib" in place of this
   implementation.

   "xalloc_die()" was inspired from gnulib
   <www.gnu.org/software/gnulib>.  Memory size checking in this code
   was inspired from the DJGPP <www.delorie.com/djgpp> xalloc
   functions, courtesy of DJ Delorie.  */

#include <stdio.h>
#include <stdlib.h>

/* Private Declarations */
static void xalloc_die();

void *xmalloc(size_t num)
{
	void *mem;
	mem = malloc(num ? num : 1);
	if (mem == NULL)
		xalloc_die();
	return mem;
}

void *xrealloc(void *mem, size_t num)
{
	if (mem == NULL)
		mem = malloc(num ? num : 1);
	else
		mem = realloc(mem, num ? num : 1);
	if (mem == NULL)
		xalloc_die();
	return mem;
}

void xfree(void *mem)
{
	if (mem != NULL)
		free(mem);
}

static void xalloc_die()
{
	fprintf(stderr, "Memory exhausted.\n");
	abort();
}
