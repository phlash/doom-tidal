// Copyright(C) 2022- Phil Ashby
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	Micropython interop layer: starts the game, handles player
//	control inputs, WAD file input, video blitting. Provides any
//	missing POSIX / libc functions expected by doomgeneric..
//

// we include the structure definition for mp_fun_table_t but NOT
// the declaration of an instance of it (in py/dynruntime.h) or the
// inline functions & macros that rely on the instance. This allows
// the use of a pointer passed in from the true module (doomloader.c)
// and avoids linker issues or a need for special compiler options
#include "py/nativeglue.h"
// fugly hack to prevent collision of stdbool types and doom internals
#undef true
#undef false
#include "m_argv.h"
#include "doomgeneric.h"

void D_DoomMain (void);

void M_FindResponseFile(void);


// == linker defined symbols we use to clear .bss
extern int _bss_start, _end;
// == uPython callback object (an instance of the loader module)
static mp_obj_t dw_callback;
// == dynamic pointer to uPython module API helper functions
//    plus some macros to keep code tidy (derived from dynruntime.h)
static mp_fun_table_t *dw_fun_table;
#define mp_plat_print						(*dw_fun_table->plat_print)
#define mp_printf(p, ...)					(dw_fun_table->printf_((p), __VA_ARGS__))
#define mp_vprintf(p, f, a)					(dw_fun_table->vprintf_((p), (f), (a)))
#define mp_obj_none							(dw_fun_table->const_none)
#define mp_obj_new_str(s, l)				(dw_fun_table->obj_new_str((s), (l)))
#define mp_obj_new_int(n)					(dw_fun_table->native_to_obj((n), MP_NATIVE_TYPE_INT))
#define mp_obj_new_bytearray_by_ref(p, l)	(dw_fun_table->obj_new_bytearray_by_ref((p), (l)))
#define mp_call_function_n_kw(f, na, nk, a)	(dw_fun_table->call_function_n_kw((f), (na)|((nk)<<8), (a)))
#define mp_obj_get_int(o)					(dw_fun_table->native_from_obj((o), MP_NATIVE_TYPE_INT))

#define m_malloc(s)							(dw_fun_table->realloc_(NULL, (s), false))
#define m_free(p)							(dw_fun_table->realloc_((p), 0, false))
#define m_realloc(p, s)						(dw_fun_table->realloc_((p), (s), true))

// == jump buffer, used to exit Doom, size definitely larger than any offset found in ROM disassembly..
static int dw_exit[64];
#define ESPROM_SETJMP	0x4000144c
#define ESPROM_LONGJMP	0x40001440

// == Our entry point, called by the loader
void run_doom(mp_obj_t callback, mp_fun_table_t *fun_table)
{
	// clear .bss (nb: cannot use mp_printf macro yet..)
	unsigned char *bss = (unsigned char *)&_bss_start;
	unsigned char *end = (unsigned char *)&_end;
	fun_table->printf_(fun_table->plat_print, "clearing .bss (%p-%p)..\n", bss, end);
	while (bss<end)
		*bss++ = 0;
	fun_table->printf_(fun_table->plat_print, "done\n");

    // fake arguments
	char *argv[] = { "doom", NULL };
    myargc = 1;
    myargv = argv;

	// save callback for library functions below..
	dw_callback = callback;

	// save function table for functions below..
	dw_fun_table = fun_table;

	// locate and process arguments from '@' file (if any)
    M_FindResponseFile();

    // start doom
    mp_printf(&mp_plat_print, "Starting D_DoomMain\r\n");

	// non-local exit point
	int rv = ((int (*)(int*))ESPROM_SETJMP)(dw_exit);
	if (!rv)
		D_DoomMain ();

    mp_printf(&mp_plat_print, "Exit from D_DoomMain: %d\r\n", rv);
    
	// clear callback & function table
	dw_callback = 0;
	dw_fun_table = NULL;
}


// == DoomGeneric porting functions..
void DG_Init(void) {
	// nothing to do yet..
	mp_printf(&mp_plat_print, "DG_Init\n");
}

void DG_DrawFrame(void) {
	// blit DG_ScreenBuffer - by calling back to python(!)
#if DOOMGENERIC_RESY!=150
#error Cannot use resolutions other than 240x135 for TiDAL badge, soz!
#endif
	mp_obj_t args[2] = {
		mp_obj_new_str("blit", 4),
		mp_obj_new_bytearray_by_ref(DOOMGENERIC_RESX*(DOOMGENERIC_RESY-15)*2, DG_ScreenBuffer),
	};
	mp_call_function_n_kw(dw_callback, 2, 0, &args[0]);
}

void DG_SleepMs(uint32_t ms) {
	mp_obj_t args[2] = {
		mp_obj_new_str("sleep", 5),
		mp_obj_new_int(ms),
	};
	mp_call_function_n_kw(dw_callback, 2, 0, &args[0]);
}

uint32_t DG_GetTicksMs(void) {
	mp_obj_t args[1] = {
		mp_obj_new_str("ticks", 5),
	};
	return mp_obj_get_int(mp_call_function_n_kw(dw_callback, 2, 0, &args[0]));
}

int DG_GetKey(int *pressed, unsigned char *key) {
	return 0;
}

void DG_SetWindowTitle(const char *title) {
	mp_printf(&mp_plat_print, "DG_SetWindowTitle\n");
}

// == missing POSIX / libc functions..

int dw_errno;
int *__errno(void) {
	return &dw_errno;
}

#include <ctype.h>
// 'borrowed' from newlib source..
#define _CTYPE_DATA_0_127 \
	_C,	_C,	_C,	_C,	_C,	_C,	_C,	_C, \
	_C,	_C|_S, _C|_S, _C|_S,	_C|_S,	_C|_S,	_C,	_C, \
	_C,	_C,	_C,	_C,	_C,	_C,	_C,	_C, \
	_C,	_C,	_C,	_C,	_C,	_C,	_C,	_C, \
	_S|_B,	_P,	_P,	_P,	_P,	_P,	_P,	_P, \
	_P,	_P,	_P,	_P,	_P,	_P,	_P,	_P, \
	_N,	_N,	_N,	_N,	_N,	_N,	_N,	_N, \
	_N,	_N,	_P,	_P,	_P,	_P,	_P,	_P, \
	_P,	_U|_X,	_U|_X,	_U|_X,	_U|_X,	_U|_X,	_U|_X,	_U, \
	_U,	_U,	_U,	_U,	_U,	_U,	_U,	_U, \
	_U,	_U,	_U,	_U,	_U,	_U,	_U,	_U, \
	_U,	_U,	_U,	_P,	_P,	_P,	_P,	_P, \
	_P,	_L|_X,	_L|_X,	_L|_X,	_L|_X,	_L|_X,	_L|_X,	_L, \
	_L,	_L,	_L,	_L,	_L,	_L,	_L,	_L, \
	_L,	_L,	_L,	_L,	_L,	_L,	_L,	_L, \
	_L,	_L,	_L,	_P,	_P,	_P,	_P,	_C

#define _CTYPE_DATA_128_255 \
	0,	0,	0,	0,	0,	0,	0,	0, \
	0,	0,	0,	0,	0,	0,	0,	0, \
	0,	0,	0,	0,	0,	0,	0,	0, \
	0,	0,	0,	0,	0,	0,	0,	0, \
	0,	0,	0,	0,	0,	0,	0,	0, \
	0,	0,	0,	0,	0,	0,	0,	0, \
	0,	0,	0,	0,	0,	0,	0,	0, \
	0,	0,	0,	0,	0,	0,	0,	0, \
	0,	0,	0,	0,	0,	0,	0,	0, \
	0,	0,	0,	0,	0,	0,	0,	0, \
	0,	0,	0,	0,	0,	0,	0,	0, \
	0,	0,	0,	0,	0,	0,	0,	0, \
	0,	0,	0,	0,	0,	0,	0,	0, \
	0,	0,	0,	0,	0,	0,	0,	0, \
	0,	0,	0,	0,	0,	0,	0,	0, \
	0,	0,	0,	0,	0,	0,	0,	0

const char _ctype_[1 + 256] = {
	0,
	_CTYPE_DATA_0_127,
	_CTYPE_DATA_128_255
};

void exit(int code) {
	// ..use saved non-local return buffer to bail
	mp_printf(&mp_plat_print, "*** doom called exit(%d) - jumping out.\n", code);
	if (code>0)
		code = -code;
	else if (!code)
		code = 1;
	((void (*)(int*, int))ESPROM_LONGJMP)(dw_exit, code);
	while (1);
}

int system(const char *cmd) {
	mp_printf(&mp_plat_print, "system(%s)=-1\n", cmd);
	// not suppported
	return -1;
}

int printf(const char *f, ...) {
	va_list ap;
	va_start(ap, f);
	int n = mp_vprintf(&mp_plat_print, f, ap);
	va_end(ap);
	return n;
}

int vprintf(const char *f, va_list ap) {
	int r = mp_vprintf(&mp_plat_print, f, ap);
	return r;
}

int puts(const char *s) {
	int r = mp_printf(&mp_plat_print, "%s\n", s);
	return r;
}

int putchar(int c) {
	mp_printf(&mp_plat_print, "%c", (char)c);
	return c;
}

struct _spdata { char *buf; size_t len; size_t pos; };
void _snprintf(void *p, const char *str, size_t len) {
	struct _spdata *data = (struct _spdata *)p;
	size_t i;
	for (i=0; i+data->pos<data->len && i<len; i++)
		data->buf[i+data->pos] = str[i];
	if (i+data->pos<data->len)
		data->buf[i+data->pos]=0;
	else
		mp_printf(&mp_plat_print, "_snprintf() buffer overflow: %d\n", data->len);
	data->pos += i;
}
int vsnprintf(char *buf, size_t len, const char *fmt, va_list ap) {
	struct _spdata data = { buf, len, 0 };
	mp_print_t us = { &data, _snprintf };
	mp_vprintf(&us, fmt, ap);
	return (int)data.pos;
}
int snprintf(char *buf, size_t len, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int r = vsnprintf(buf, len, fmt, ap);
	va_end(ap);
	return r;
}

int atoi(const char *s) {
	int r = 0;
	while (*s) {
		if (*s<'0' || *s>'9')
			break;
		r *= 10;
		r += (int)(*s-'0');
		s++;
	}
	return r;
}

double atof(const char *s) {
	double r = 0.0;
	double m = 1.0;
	bool dot = false;
	while (*s) {
		if ('.' == *s) {
			dot = true;
			continue;
		} else if (*s<'0' || *s>'9') {
			break;
		}
		if (dot) {
			m /= 10.0;
			r += (double)(*s-'0')*m;
		} else {
			r *= 10.0;
			r += (double)(*s-'0');
		}
	}
	return r;
}

int sscanf(const char *s, const char *f, ...) {
	// TODO:PAA - uPython doesn't give us any route to the real scanf :-(
	printf("sscanf(%s, ...) = -1 :(\n", f);
	return -1;
}

void *memset(void *s, int c, size_t n) {
	return dw_fun_table->memset_(s, c, n);
}

void *malloc(size_t l) {
	void *p = m_malloc(l);
	mp_printf(&mp_plat_print, "malloc(%u)=%p\n", l, p);
	return p;
}

void *calloc(size_t n, size_t l) {
	void *p = malloc(n*l);
	if (p)
		memset(p, 0, n*l);
	return p;
}

void *realloc(void *p, size_t l) {
	return m_realloc(p, l);
}

void free(void *p) {
	m_free(p);
}

void *memmove(void *d, const void *s, size_t n) {
	return dw_fun_table->memmove_(d, s, n);
}

void *memcpy(void *d, const void *s, size_t n) {
	return memmove(d, s, n);
}

size_t strlen(const char *s) {
	size_t i=0;
	while (s[i]) i++;
	return i;
}

char *strncpy(char *d, const char *s, size_t n) {
	size_t i;
	for (i=0; i<n && s[i]; i++)
		d[i] = s[i];
	while (i<n)
		d[i++]=0;
	return d;
}

int strcmp(const char *s1, const char *s2) {
	int i=-1;
	do {
		++i;
		if (s1[i]<s2[i])
			return -1;
		else if (s1[i]>s2[i])
			return 1;
	} while (s1[i] && s2[i]);
	return 0;
}

int strncmp(const char *s1, const char *s2, size_t n) {
	int i=-1;
	do {
		++i;
		if (s1[i]<s2[i])
			return -1;
		else if (s1[i]>s2[i])
			return 1;
	} while (i+1<n && s1[i] && s2[i]);
	return 0;
}

int strcasecmp(const char *s1, const char *s2) {
	int i=-1;
	do {
		++i;
		char l1 = tolower(s1[i]);
		char l2 = tolower(s2[i]);
		if (l1<l2)
			return -1;
		else if (l1>l2)
			return 1;
	} while(s1[i] && s2[i]);
	return 0;
}

int strncasecmp(const char *s1, const char *s2, size_t n) {
	int i=-1;
	do {
		++i;
		char l1 = (s1[i]>='A' && s1[i]<='Z') ? s1[i]-'A'+'a' : s1[i];
		char l2 = (s2[i]>='A' && s2[i]<='Z') ? s2[i]-'A'+'a' : s2[i];
		if (l1<l2)
			return -1;
		else if (l1>l2)
			return 1;
	} while(i+1<n && s1[i] && s2[i]);
	return 0;
}

char *strstr(const char *s, const char *n) {
	size_t nl = strlen(n);
	while (*s) {
		if (0==strncmp(s, n, nl))
			return (char *)s;
		s++;
	}
	return NULL;
}

char *strdup(const char *s) {
	size_t l = strlen(s);
	void *p = m_malloc(l+1);
	if (!p)
		return NULL;
	return (char *)memcpy(p, s, l+1);
}

char *strchr(const char *s, int c) {
	int l = (int)strlen(s)+1;
	for(int i=0; i<l; i++) {
		if ((char)c == s[i])
			return (char *)s+i;
	}
	return NULL;
}

char *strrchr(const char *s, int c) {
	int l = (int)strlen(s) + 1;	// POSIX says we include terminator
	while (l>=0) {
		if ((char)c == s[l])
			return (char *)s+l;
		--l;
	}
	return NULL;
}

// File I/O is special, we need to call back into uPython to access the file system..
// we also fake our own FILE structure (and hope that doom doesn't monkey about in it!)
struct _fileio;
typedef struct _fileio FILE;
FILE *fopen(const char *name, const char *mode) {
	mp_obj_t args[3] = {
		 mp_obj_new_str("fopen", 5),
		 mp_obj_new_str(name, strlen(name)),
		 mp_obj_new_str(mode, strlen(mode)),
	};
	mp_obj_t rv = mp_call_function_n_kw(dw_callback, 3, 0, &args[0]);
	if (rv != mp_obj_none)
		return MP_OBJ_TO_PTR(rv);
	return NULL;
}

int fclose(FILE *stream) {
	mp_obj_t args[2] = {
		mp_obj_new_str("fclose", 6),
		MP_OBJ_FROM_PTR(stream),
	};
	mp_obj_t rv = mp_call_function_n_kw(dw_callback, 2, 0, &args[0]);
	if (rv != mp_obj_none)
		return mp_obj_get_int(rv);
	return -1;
}

long ftell(FILE *stream) {
	mp_obj_t args[2] = {
		mp_obj_new_str("ftell", 5),
		MP_OBJ_FROM_PTR(stream),
	};
	mp_obj_t rv = mp_call_function_n_kw(dw_callback, 2, 0, &args[0]);
	if (rv != mp_obj_none)
		return mp_obj_get_int(rv);
	return -1;
}

int fseek(FILE *stream, long o, int w) {
	mp_obj_t args[4] = {
		mp_obj_new_str("fseek", 5),
		MP_OBJ_FROM_PTR(stream),
		mp_obj_new_int(o),
		mp_obj_new_int(w),
	};
	mp_obj_t rv = mp_call_function_n_kw(dw_callback, 4, 0, &args[0]);
	if (rv != mp_obj_none)
		return mp_obj_get_int(rv);
	return -1;
}

size_t fwrite(const void *buf, size_t sz, size_t num, FILE *stream) {
	mp_obj_t args[3] = {
		mp_obj_new_str("fwrite", 6),
		mp_obj_new_bytearray_by_ref(sz*num, (void *)buf),
		MP_OBJ_FROM_PTR(stream),
	};
	mp_obj_t rv = mp_call_function_n_kw(dw_callback, 3, 0, &args[0]);
	if (rv != mp_obj_none) {
		int wr = mp_obj_get_int(rv);
		return wr/sz;
	}
	return 0;
}

size_t fread(void *buf, size_t sz, size_t num, FILE *stream) {
	mp_obj_t args[3] = {
		mp_obj_new_str("fread", 5),
		mp_obj_new_bytearray_by_ref(sz*num, buf),
		MP_OBJ_FROM_PTR(stream),
	};
	mp_obj_t rv = mp_call_function_n_kw(dw_callback, 3, 0, &args[0]);
	if (rv != mp_obj_none) {
		int rd = mp_obj_get_int(rv);
		return rd/sz;
	}
	return 0;
}

int fputs(const char *s, FILE *f) {
	return fwrite(s, 1, strlen(s), f);
}

struct _fpdata { FILE *f; int len; };
void _fprintf(void *p, const char *str, size_t len) {
	struct _fpdata *d = (struct _fpdata *)p;
	d->len = fwrite(str, 1, len, d->f);
}
int vfprintf(FILE *f, const char *fmt, va_list ap) {
	struct _fpdata d = { f, 0 };
	mp_print_t us = { &d, _fprintf };
	mp_printf(&us, fmt, ap);
	return d.len;
}
int fprintf(FILE *p, const char *f, ...) {
	va_list ap;
	va_start(ap, f);
	int r = vfprintf(p, f, ap);
	va_end(ap);
	return r;
}

int remove(const char *path) {
	mp_obj_t args[2] = {
		mp_obj_new_str("remove", 6),
		mp_obj_new_str(path, strlen(path)),
	};
	mp_obj_t rv = mp_call_function_n_kw(dw_callback, 2, 0, &args[0]);
	if (rv != mp_obj_none)
		return mp_obj_get_int(rv);
	return -1;
}

int rename(const char *old, const char *new) {
	mp_obj_t args[3] = {
		mp_obj_new_str("rename", 6),
		mp_obj_new_str(old, strlen(old)),
		mp_obj_new_str(new, strlen(new)),
	};
	mp_obj_t rv = mp_call_function_n_kw(dw_callback, 3, 0, &args[0]);
	if (rv != mp_obj_none)
		return mp_obj_get_int(rv);
	return -1;
}

int mkdir(const char *path, int mode) {
	mp_obj_t args[3] = {
		mp_obj_new_str("mkdir", 5),
		mp_obj_new_str(path, strlen(path)),
		mp_obj_new_int(mode),
	};
	mp_obj_t rv = mp_call_function_n_kw(dw_callback, 3, 0, &args[0]);
	if (rv != mp_obj_none)
		return mp_obj_get_int(rv);
	return -1;
}

