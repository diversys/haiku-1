#ifndef CPP_H
#define CPP_H
/* cpp - C++ in the kernel
**
** Initial version by Axel Dörfler, axeld@pinc-software.de
** This file may be used under the terms of the OpenBeOS License.
*/


#include <new>
#include <stdlib.h>


// Oh no! C++ in the kernel! Are you nuts?
//
//	- no exceptions
//	- (almost) no virtuals (well, the Query code now uses them)
//	- it's basically only the C++ syntax, and type checking
//	- since one tend to encapsulate everything in classes, it has a slightly
//	  higher memory overhead
//	- nicer code
//	- easier to maintain


inline void *
operator new(size_t size)
{
	return malloc(size);
} 


inline void *
operator new[](size_t size)
{
	return malloc(size);
}
 

inline void
operator delete(void *ptr)
{
	free(ptr);
} 


inline void
operator delete[](void *ptr)
{
	free(ptr);
}

// we're using virtuals
extern "C" void __pure_virtual();


#endif	/* CPP_H */
