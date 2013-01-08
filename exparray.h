/* A simple expandable array implementation.

   How to use:

   * This expandable array implementation is divided into two
     sections: core primitives and convenience functions.

   * Whenever you want to use a specific type in an expandable array,
	 first call `EA_TYPE(typename)' with your type.  This macro
	 defines a structure customized to be an array of that specific
	 type.  The first field of the structure, `d', is a pointer of the
	 given type, which points to the dynamically allocated array
	 contents.

   * When you want to declare an expandable array, simply use
	 `typename_array' as the type name (with your type as `typename',
	 of course).

   * Initialize the array before use by calling `EA_INIT()'.  `array'
	 is the value of your array (use the syntax `*array' if it is a
	 pointer), and `reserve' is the number of elements to allocate
	 space for in advance.  If linear reallocation is being used, then
	 the array will be reallocated with that many more elements when
	 the array becomes full.

   * All of these macros accept values rather than pointers, so use
     syntax as described above if you have a pointer that will be used
     as an argument.

   * NOTE: Because of the fact that this expandable array
     implementation uses macros extensively, there are some
     programming situations where macro inputs get clobbered during
     intermediate execution of the macros.  So far, this code was only
     modified in one place to compensate for these issues.

   * When you are done using an expandable array, make sure you call
     `EA_DESTROY()'.

   * To reserve space for extra elements that will be written in-place
     at a later time, set `typename_array::len' to the desired size
     and then call `EA_NORMALIZE()'.  `EA_NORMALIZE()' can also be
     used to free up unused space in an array.

   * The normal behavior of an exparray is to allocate a reserve in
     advance, and allocate more space once all of the previously
     pre-allocated space is filled up.  Thus, one way to append a
     single element to the array is to write to the position at
     `typename_array::d[typename_array::len]', then call `EA_ADD()'.

   * You can change which functions are used for allocation by
     defining `ea_malloc', `ea_realloc', and `ea_free' to the
     functions to use for allocation.

   * You can also modify allocation behavior by providing different
     macros for `EA_GROW()' and `EA_NORMALIZE()'.  Normally,
     allocation behavior is done in an exponential manner without
     clearing newly allocated memory.  You can change this by defining
     `EA_LINEAR_REALLOC' or `EA_GARRAY_REALLOC'.

   * The rest of the functions are convenience functions that use the
     previously mentioned primitives.  See their individual
     documentation for more details.

   * There is a wrapper header available for you to use this
     implementation as a backend in place of the real GLib GArray
     implementation.
*/

#ifndef EXPARRAY_H
#define EXPARRAY_H

#ifndef ea_malloc
#include <stdlib.h>
#define ea_malloc  malloc
#define ea_realloc realloc
#define ea_free    free
#endif

#include <string.h>

#define EA_TYPE(typename)						\
	struct typename##_array_tag					\
	{											\
		typename *d;							\
		unsigned len;							\
		unsigned tysize;						\
		/* User-defined fields.  */				\
		unsigned user1;							\
	};											\
	typedef struct typename##_array_tag typename##_array;

/* Aliases for user-defined fields.  */
#define ea_len_alloc user1

/* Initialize the given array.

   Parameters:

   `typename'    the base type that the array contains, such as `int'
   `array'       the value (not pointer) of the array to modify
   `reserve'     the amount of memory to pre-allocate.  The length of
                 the array is always initialized to zero. */
#define EA_INIT(typename, array, reserve)						\
{																\
	(array).len = 0;											\
	(array).tysize = sizeof(typename);							\
	(array).ea_len_alloc = reserve;								\
	(array).d = (typename *)ea_malloc((array).tysize *			\
									  (array).ea_len_alloc);	\
}

/* Destroy the given array.  This is mostly just a convenience
   function.  */
#define EA_DESTROY(typename, array)					\
{													\
	if ((array).d != NULL)							\
		ea_free((array).d);							\
	(array).d = NULL;								\
	(array).len = 0;								\
	(array).tysize = 0;								\
	(array).user1 = 0;								\
}

/* Reallocators:

   EA_GROW(typename, array)
     Reallocation function for single step growth.  The `len' member
     of the array should have already been incremented.

   EA_NORMALIZE(typename, array)
     Ensure that the allocated space is consistent with the current
     allocation algorithm.  */

/* GLib GArray reallocators.  */
#ifdef EA_GARRAY_REALLOC
#define EA_GROW(typename, array)										\
{																		\
	struct _GRealWrap *reala = (struct _GRealWrap *)(&array);			\
	if (reala->len + (reala->zero_term ? 1 : 0) >= reala->ea_len_alloc) \
	{																	\
		reala->ea_len_alloc *= 2;										\
		reala->data = (typename *)ea_realloc(reala->data, reala->tysize * \
											 reala->ea_len_alloc);		\
		if (reala->clear)												\
		{																\
			memset(reala->data + reala->tysize * reala->len, 0,			\
				   reala->tysize * (reala->ea_len_alloc - reala->len)); \
		}																\
		else if (reala->zero_term)										\
		{																\
			memset(reala->data + reala->tysize * reala->len, 0,			\
				   reala->tysize * 1);									\
		}																\
	}																	\
}
#define EA_NORMALIZE(typename, array)								\
{																	\
	struct _GRealWrap *reala = (struct _GRealWrap *)(&array);		\
	while (reala->ea_len_alloc >= reala->len + (reala->zero_term ? 1 : 0)) \
		reala->ea_len_alloc /= 2;									\
	if ((reala)->ea_len_alloc == 0)									\
		(reala)->ea_len_alloc = 1;									\
	while (reala->len + (reala->zero_term ? 1 : 0) >= reala->ea_len_alloc) \
		reala->ea_len_alloc *= 2;									\
	reala->data = (typename *)ea_realloc(reala->data, reala->tysize *	\
										 reala->ea_len_alloc);		\
	if (reala->clear)												\
	{																\
		memset(reala->data + reala->tysize * reala->len, 0,			\
			   reala->tysize * (reala->ea_len_alloc - reala->len)); \
	}																\
	else if (reala->zero_term)										\
	{																\
		memset(reala->data + reala->tysize * reala->len, 0,			\
			   reala->tysize * 1);									\
	}																\
}
/* END EA_GARRAY_REALLOC */

/* Linear reallocators.  */
#elif EA_LINEAR_REALLOC
#define ea_stride user1 /* User-defined field alias */
#define EA_GROW(typename, array)										\
{																		\
	if ((array).len % (array).ea_stride == 0)							\
		(array).d = (typename *)ea_realloc((array).d, (array).tysize *	\
									   ((array).len + (array).ea_stride)); \
}
#define EA_NORMALIZE(typename, array)									\
	(array).d = (typename *)ea_realloc((array).d, (array).tysize *		\
	   ((array).len + ((array).ea_stride - (array).len % (array).ea_stride)));
/* END EA_LINEAR_REALLOC */

/* Default exponential reallocators.  */
#else
#define EA_GROW(typename, array)				\
	if ((array).len >= (array).ea_len_alloc)	\
	{											\
		(array).ea_len_alloc *= 2;				\
		(array).d = (typename *)ea_realloc((array).d, (array).tysize *	\
									   (array).ea_len_alloc);		\
	}
#define EA_NORMALIZE(typename, array)								\
{																	\
	while ((array).ea_len_alloc > 0 &&								\
		   (array).ea_len_alloc >= (array).len)						\
		(array).ea_len_alloc /= 2;									\
	if ((array).ea_len_alloc == 0)									\
		(array).ea_len_alloc = 1;									\
	while ((array).len >= (array).ea_len_alloc)						\
		(array).ea_len_alloc *= 2;									\
	(array).d = (typename *)ea_realloc((array).d, (array).tysize *	\
								   (array).ea_len_alloc);			\
}
#endif /* END reallocators */

/*********************************************************************
   The few necessary primitive functions were specified above.  Next
   follows the convenience functions.  */

/* The notation (array).d[x] was previously used in these helper
   functions, but after I created the GLib GArray wrapper, that
   mechanism is no longer safe.  Use this "safe referencing" macro
   instead.  */
#ifdef EA_GARRAY_REALLOC
#define EA_SR(array, x) (*((array).d + (array).tysize * (x)))
#else
#define EA_SR(array, x) (array).d[x]
#endif

/* Increment the size of the array and allocate space for one new item
   in the array.  The expected calling convention of this form is to
   first set the value of the new element in the array by doing
   something like `array.d[array.num] = value;', then calling
   `EA_ADD()'.  Expandable arrays always reserve space for one item
   beyond the current size of the array.

   Parameters:

   `typename'    the base type that the array contains, such as `int'
   `array'       the value (not pointer) of the exparray to modify */
#define EA_ADD(typename, array)					\
{												\
	(array).len++;								\
	EA_GROW(typename, array);					\
}

/* Increment the size of the array, allocate space for one new item,
   and move elements in the array to make space for inserting an item
   at the given position.  The expected calling convention of this
   form is similar to that of `EA_ADD()', only the value of the item
   should be set after this call rather than before it.

   Parameters:

   `typename'    the base type that the array contains, such as `int'
   `array'       the value (not pointer) of the exparray to modify
   `pos'         the zero-based index of where the new item will be
                 inserted */
#define EA_INS(typename, array, pos)			\
{												\
	EA_ADD(typename, array);					\
	memmove(&EA_SR(array, pos+1), &EA_SR(array, pos), (array).tysize * \
			((array).len - pos));									   \
}

/* Appends the given item to the end of the array.  Note that the
   appended item cannot be larger than an integer type.  To append a
   larger item, use `EA_APPEND_MULT()' with the quantity set to one.

   Parameters:

   `typename'    the base type that the array contains, such as `int'
   `array'       the value (not pointer) of the exparray to modify
   `element'     the value of the item to append */
#define EA_APPEND(typename, array, element)								\
{																		\
	EA_SR(array, (array).len) = element;								\
	EA_ADD(typename, array);											\
}

/* Inserts the given item at the indicated position.  Elements after
   this position will get moved back.  Note that the inserted item
   cannot be larger than an integer type.  To insert a larger item,
   use `EA_INSERT_MULT()' with the quantity set to one.

   Parameters:

   `typename'    the base type that the array contains, such as `int'
   `array'       the value (not pointer) of the exparray to modify
   `pos'         the zero-based index indicating where to insert the
                 given element
   `element'     the value of the item to append */
#define EA_INSERT(typename, array, pos, element)						\
{																		\
	unsigned sos = pos; /* avoid macro evaluation problems */			\
	EA_INS(typename, array, sos);										\
	EA_SR(array, sos) = element;										\
}

/* Append multiple items at one time.

   Parameters:

   `typename'    the base type that the array contains, such as `int'
   `array'       the value (not pointer) of the exparray to modify
   `data'        the address of the items to append
   `num_len'     the number of items to append */
#define EA_APPEND_MULT(typename, array, data, num_data)					\
	if (data != NULL)													\
	{																	\
		unsigned pos = (array).len;										\
		(array).len += num_data;										\
		EA_NORMALIZE(typename, array);									\
		memcpy(&EA_SR(array, pos), data, (array).tysize * num_data);	\
	}

/* Insert multiple items at one time.

   Parameters:

   `typename'    the base type that the array contains, such as `int'
   `array'       the value (not pointer) of the exparray to modify
   `pos'         the zero-based index indicating where to insert the
                 given element
   `data'        the address of the items to insert
   `num_data'    the number of items to insert */
#define EA_INSERT_MULT(typename, array, pos, data, num_data)			\
	if (data != NULL)													\
	{																	\
		(array).len += num_data;										\
		EA_NORMALIZE(typename, array);									\
		memmove(&EA_SR(array, pos+num_data), &EA_SR(array, pos),		\
				(array).tysize * ((array).len - pos));					\
		memcpy(&EA_SR(array, pos), data, (array).tysize * num_data);	\
	}

/* Prepend the given element at the beginning of the array.  Note that
   the prepended item cannot be larger than an integer type.  To
   prepend a larger item, use `EA_PREPEND_MULT()' with the quantity
   set to one.

   Parameters:

   `typename'    the base type that the array contains, such as `int'
   `array'       the value (not pointer) of the exparray to modify
   `element'     the value of the element to append */
#define EA_PREPEND(typename, array, element) \
	EA_INSERT(typename, array, 0, element);

/* Prepend multiple elements at one time.

   Parameters:

   `typename'    the base type that the array contains, such as `int'
   `array'       the value (not pointer) of the exparray to modify
   `data'        the address of the elements to prepend
   `num_data'    the number of items to prepend */
#define EA_PREPEND_MULT(typename, array, data, num_data)	\
	EA_INSERT_MULT(typename, array, 0, data, num_data);

/* Delete the indexed element by moving following elements over.  The
   allocated array memory is not shrunken.

   Parameters:

   `typename'    the base type that the array contains, such as `int'
   `array'       the value (not pointer) of the exparray to modify
   `pos'         the zero-based index indicating which element to
                 remove */
#define EA_REMOVE(typename, array, pos)									\
{																		\
	memmove(&EA_SR(array, pos), &EA_SR(array, pos+1), (array).tysize *	\
			((array).len - (pos + 1)));									\
	(array).len--;														\
}

/* Delete the indexed element by moving the last element into the
   indexed position.  The allocated array memory is not shrunken.

   Parameters:

   `typename'    the base type that the array contains, such as `int'
   `array'       the value (not pointer) of the exparray to modify
   `pos'         the zero-based index indicating which element to
                 remove */
#define EA_REMOVE_FAST(typename, array, pos)							\
{																		\
	memmove(&EA_SR(array, pos), &EA_SR(array, (array).len-1),			\
			(array).tysize);											\
	(array).len--;														\
}

/* Remove a range of elements at index `pos' of length `len'.

   Parameters:

   `typename'    the base type that the array contains, such as `int'
   `array'       the value (not pointer) of the exparray to modify
   `data'        the address of the data array to append
   `pos'         the zero-based index indicating the first element to
                 remove
   `num_data'    the number of items to remove */
#define EA_REMOVE_RANGE(typename, array, pos, num_data)					\
{																		\
	memmove(&EA_SR(array, pos), &EA_SR(array, pos+num_data),			\
			(array).tysize * ((array).len - (pos + num_data)));			\
}

/* Set an array's size and allocate enough memory for that size.

   Parameters:

   `typename'    the base type that the array contains, such as `int'
   `array'       the value (not pointer) of the exparray to modify
   `size'        the new size of the array, measured in elements */
#define EA_SET_SIZE(typename, array, size)			\
{													\
	(array).len = size;								\
	EA_NORMALIZE(typename, array);					\
}

/* Push an element onto the end of the given array, as if it were a
   stack.  This function is just an alias for `EA_APPEND()'.  */
#define EA_PUSH_BACK(typename, array, element) \
	EA_APPEND(typename, array, element);

/* Pop an element off of the end of the given array, as if it were a
   stack.  The allocated array memory is not shrunken.  */
#define EA_POP_BACK(typename, array) \
	(array).len--;

/* This function is just an alias for `EA_PREPEND()'.  You should not
   use this macro for a stack data structure, as it will have serious
   performance problems when used in that way.  */
#define EA_PUSH_FRONT(typename, array, element) \
	EA_PREPEND(typename, array, element);

/* Remove an element from the beginning of the given array.  The
   allocated array memory is not shrunken.  You should not use this
   macro for a stack data structure, as it will have serious
   performance problems when used in that way.  */
#define EA_POP_FRONT(typename, array) \
	EA_REMOVE(typename, array, 0);

#endif /* not EXPARRAY_H */
