#ifndef _LINUX_386_MF_RUNTIME
#define _LINUX_386_MF_RUNTIME
#ifdef _MUDFLAP
#pragma redefine_extname memzero kmudflap_memzero
#endif /* _MUDFLAP */
#endif
