								/* For Debugging purposes*/
/* Comment out the required macro and debug statements will be printed of the appropriate file*/

//#define DEBUG_EXCEPTION
//#define DEBUG_PAGE
//#define DEBUG_SWAP
//#define DEBUG_FRAME
//#define DEBUG_PROCESS
//#define DEBUG_MMF
//#define DEBUG_SYSCALL
//#define DEBUG_CACHE

#ifndef DEBUG_EXCEPTION
	#define DPRINT_EXCEP(str,arg)
	#define DPRINTF_EXCEP(str)
#endif
#ifdef DEBUG_EXCEPTION
	#define DPRINT_EXCEP(str,arg) printf(str,arg)
	#define DPRINTF_EXCEP(str) printf(str)
#endif


#ifdef DEBUG_PAGE
	#define DPRINT_PAGE(str,arg) printf(str,arg)
	#define DPRINTF_PAGE(str) printf(str)
#endif
#ifndef DEBUG_PAGE
	#define DPRINT_PAGE(str,arg)
	#define DPRINTF_PAGE(str)
#endif

#ifdef DEBUG_SWAP
	#define DPRINT_SWAP(str,arg) printf(str,arg)
	#define DPRINTF_SWAP(str) printf(str)
#endif
#ifndef DEBUG_SWAP
	#define DPRINT_SWAP(str,arg)
	#define DPRINTF_SWAP(str)
#endif


#ifdef DEBUG_FRAME
	#define DPRINT_FRAME(str,arg) printf(str,arg)
	#define DPRINTF_FRAME(str) printf(str)
#endif
#ifndef DEBUG_FRAME
	#define DPRINT_FRAME(str,arg)
	#define DPRINTF_FRAME(str)
#endif

#ifdef DEBUG_PROCESS
	#define DPRINT_PROC(str,arg) printf(str,arg)
	#define DPRINTF_PROC(str) printf(str)
#endif
#ifndef DEBUG_PROCESS
	#define DPRINT_PROC(str,arg)
	#define DPRINTF_PROC(str)
#endif

#ifdef DEBUG_SYSCALL
	#define DPRINT_SYS(str,arg) printf(str,arg)
	#define DPRINTF_SYS(str) printf(str)
#endif
#ifndef DEBUG_SYSCALL
	#define DPRINT_SYS(str,arg)
	#define DPRINTF_SYS(str)
#endif

#ifdef DEBUG_MMF
	#define DPRINT_MMF(str,arg) printf(str,arg)
	#define DPRINTF_MMF(str) printf(str)
#endif
#ifndef DEBUG_MMF
	#define DPRINT_MMF(str,arg)
	#define DPRINTF_MMF(str)
#endif

#ifdef DEBUG_CACHE
	#define DPRINT_CACHE(str,arg) printf(str,arg)
	#define DPRINTF_CACHE(str) printf(str)
#endif
#ifndef DEBUG_CACHE
	#define DPRINT_CACHE(str,arg)
	#define DPRINTF_CACHE(str)
#endif
