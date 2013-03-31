//#define DEBUG_EXCEPTION						/* For Debugging purposes*/
//#define DEBUG_PAGE
//#define DEBUG_SWAP
//#define DEBUG_FRAME
//#define DEBUG_PROCESS
//#define DEBUG_SYSCALL

#ifndef DEBUG_EXCEPTION
	#define DPRINT_EXCEP(str,arg)
	#define DPRINTF(str)
#endif
#ifdef DEBUG_EXCEPTION
	#define DPRINT_EXCEP(str,arg) printf(str,arg)
	#define DPRINTF(str) printf(str)
#endif


#ifdef DEBUG_PAGE
	#define DPRINT_PAGE(str,arg) printf(str,arg)
	#define DPRINTF(str) printf(str)
#endif
#ifndef DEBUG_PAGE
	#define DPRINT_PAGE(str,arg)
	#define DPRINTF(str)
#endif

#ifdef DEBUG_SWAP
	#define DPRINT_SWAP(str,arg) printf(str,arg)
	#define DPRINTF(str) printf(str)
#endif
#ifndef DEBUG_SWAP
	#define DPRINT_SWAP(str,arg,type)
	#define DPRINTF(str)
#endif


#ifdef DEBUG_FRAME
	#define DPRINT_FRAME(str,arg) printf(str,arg)
	#define DPRINTF(str) printf(str)
#endif
#ifndef DEBUG_FRAME
	#define DPRINT_FRAME(str,arg,type)
	#define DPRINTF(str)
#endif

#ifdef DEBUG_PROCESS
	#define DPRINT_PROC(str,arg) printf(str,arg)
	#define DPRINTF(str) printf(str)
#endif
#ifndef DEBUG_PROCESS
	#define DPRINT_PROC(str,arg,type)
	#define DPRINTF(str)
#endif

#ifdef DEBUG_SYSCALL
	#define DPRINT_SYS(str,arg) printf(str,arg)
	#define DPRINTF(str) printf(str)
#endif
#ifndef DEBUG_SYSCALL
	#define DPRINT_SYS(str,arg,type)
	#define DPRINTF(str)
#endif
