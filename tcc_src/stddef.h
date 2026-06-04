/* Minimal stddef.h for TCC — SDVM custom */
#ifndef _STDDEF_H
#define _STDDEF_H

typedef long size_t;
typedef long ptrdiff_t;
typedef short wchar_t;

#ifndef NULL
#define NULL ((void*)0)
#endif

#endif
