//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
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

#include "py/dynruntime.h"
// fugly hack to prevent collision of stdbool types and doom internals
#undef true
#undef false
#include "m_argv.h"
#include "doomgeneric.h"

void D_DoomMain (void);

void M_FindResponseFile(void);

// == Our one exported function for uPython to run doom..
STATIC mp_obj_t s_callback;
STATIC mp_obj_t doom(mp_obj_t callback)
{
    // fake arguments
	char *argv[] = { "doom", NULL };
    myargc = 1;
    myargv = argv;

	// save callback for library functions below..
	s_callback = callback;

	// not sure: PAA?
    M_FindResponseFile();

    // start doom
    mp_printf(&mp_plat_print, "Starting D_DoomMain\r\n");
    
	D_DoomMain ();

	// clear callback
	s_callback = 0;

    return mp_obj_new_int(0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(doom_obj, doom);

STATIC mp_obj_t d_memset(mp_obj_t bobj, mp_obj_t iobj)
{
	// this uses the buffer protocol to gain access to raw buffer for memset()
	mp_buffer_info_t binf;
	mp_get_buffer_raise(bobj, &binf, MP_BUFFER_WRITE);
	mp_fun_table.memset_(binf.buf, mp_obj_get_int(iobj), binf.len);
	return bobj;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(d_memset_obj, d_memset);

// == Entry point
mp_obj_t mpy_init(mp_obj_fun_bc_t *self, size_t n_args, size_t n_kw, mp_obj_t *args)
{
	MP_DYNRUNTIME_INIT_ENTRY;
	mp_store_global(MP_QSTR_doom, MP_OBJ_FROM_PTR(&doom_obj));
	mp_store_global(MP_QSTR_d_memset, MP_OBJ_FROM_PTR(&d_memset_obj));
	MP_DYNRUNTIME_INIT_EXIT;
}

// == DoomGeneric porting functions..
void DG_Init(void) {
	// nothing to do yet..
	mp_printf(&mp_plat_print, "DG_Init\n");
}

void DG_DrawFrame(void) {
	mp_printf(&mp_plat_print, "DG_DrawFrame\n");
	// blit DG_ScreenBuffer - by calling back to python(!)
#if DOOMGENERIC_RESY!=150
#error Cannot use resolutions other than 240x135 for TiDAL badge, soz!
#endif
	mp_obj_t args[2] = {
		mp_obj_new_str("blit", 4),
		mp_obj_new_bytearray_by_ref(DOOMGENERIC_RESX*(DOOMGENERIC_RESY-15)*2, DG_ScreenBuffer),
	};
	mp_call_function_n_kw(s_callback, 2, 0, &args[0]);
}

void DG_SleepMs(uint32_t ms) {
	mp_printf(&mp_plat_print, "DG_SleepMs\n");
	mp_obj_t args[2] = {
		mp_obj_new_str("sleep", 5),
		mp_obj_new_int(ms),
	};
	mp_call_function_n_kw(s_callback, 2, 0, &args[0]);
}

uint32_t DG_GetTicksMs(void) {
	mp_printf(&mp_plat_print, "DG_GetTickMs\n");
	mp_obj_t args[1] = {
		mp_obj_new_str("ticks", 5),
	};
	return mp_obj_get_int(mp_call_function_n_kw(s_callback, 2, 0, &args[0]));
}

int DG_GetKey(int *pressed, unsigned char *key) {
	mp_printf(&mp_plat_print, "DG_GetKey\n");
	return 0;
}

void DG_SetWindowTitle(const char *title) {
	mp_printf(&mp_plat_print, "DG_SetWindowTitle\n");
}

// == missing POSIX / libc functions..

static int dg_errno;
int *__errno(void) {
	return &dg_errno;
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
	// TODO:PAA - we can't exit uPython..
	// ..we /could/ longjmp back to entry??
	// ..we /could/ call back into uPython to terminate thread??
	mp_printf(&mp_plat_print, "*** doom called exit(%d) - hanging.\n", code);
	while(1);
}

int system(const char *cmd) {
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

struct _spdata { char *buf; size_t len; };
void _snprintf(void *p, const char *str, size_t len) {
	struct _spdata *data = (struct _spdata *)p;
	size_t i;
	for (i=0; i<data->len && i<len; i++)
		data->buf[i] = str[i];
	data->len = i;
}
int vsnprintf(char *buf, size_t len, const char *fmt, va_list ap) {
	struct _spdata data = { buf, len };
	mp_print_t us = { &data, _snprintf };
	mp_printf(&us, fmt, ap);
	return (int)data.len;
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
	return -1;
}

void *memset(void *s, int c, size_t n) {
	return mp_fun_table.memset_(s, c, n);
}

void *malloc(size_t l) {
	return m_malloc(l);
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
	return mp_fun_table.memmove_(d, s, n);
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
	} while (i<n && s1[i] && s2[i]);
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
// we also have our own FILE structure (and hope that doom doesn't monkey about in it!)
typedef struct _fileio {
	mp_obj_t pyfile;
} FILE;
FILE *fopen(const char *name, const char *mode) {
	mp_obj_t args[3] = {
		 mp_obj_new_str("fopen", 5),
		 mp_obj_new_str(name, strlen(name)),
		 mp_obj_new_str(mode, strlen(mode)),
	};
	FILE *stream = m_malloc(sizeof(FILE));
	if (!stream)
		return NULL;
	stream->pyfile = mp_call_function_n_kw(s_callback, 3, 0, &args[0]);
	return stream;
}

int fclose(FILE *stream) {
	mp_obj_t args[2] = {
		mp_obj_new_str("fclose", 6),
		stream->pyfile,
	};
	return mp_obj_get_int(mp_call_function_n_kw(s_callback, 2, 0, &args[0]));
}

long ftell(FILE *stream) {
	mp_obj_t args[2] = {
		mp_obj_new_str("ftell", 5),
		stream->pyfile,
	};
	return mp_obj_get_int(mp_call_function_n_kw(s_callback, 2, 0, &args[0]));
}

int fseek(FILE *stream, long o, int w) {
	mp_obj_t args[4] = {
		mp_obj_new_str("fseek", 5),
		stream->pyfile,
		mp_obj_new_int(o),
		mp_obj_new_int(w),
	};
	return mp_obj_get_int(mp_call_function_n_kw(s_callback, 4, 0, &args[0]));
}

size_t fwrite(const void *buf, size_t sz, size_t num, FILE *stream) {
	mp_obj_t args[3] = {
		mp_obj_new_str("fwrite", 6),
		mp_obj_new_bytearray_by_ref(sz*num, (void *)buf),
		stream->pyfile,
	};
	int wr = mp_obj_get_int(mp_call_function_n_kw(s_callback, 3, 0, &args[0]));
	return wr/sz;
}

size_t fread(void *buf, size_t sz, size_t num, FILE *stream) {
	mp_obj_t args[3] = {
		mp_obj_new_str("fread", 5),
		mp_obj_new_bytearray_by_ref(sz*num, buf),
		stream->pyfile,
	};
	int rd = mp_obj_get_int(mp_call_function_n_kw(s_callback, 3, 0, &args[0]));
	return rd/sz;
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
	return mp_obj_get_int(mp_call_function_n_kw(s_callback, 2, 0, &args[0]));
}

int rename(const char *old, const char *new) {
	mp_obj_t args[3] = {
		mp_obj_new_str("rename", 6),
		mp_obj_new_str(old, strlen(old)),
		mp_obj_new_str(new, strlen(new)),
	};
	return mp_obj_get_int(mp_call_function_n_kw(s_callback, 3, 0, &args[0]));
}

int mkdir(const char *path, int mode) {
	mp_obj_t args[3] = {
		mp_obj_new_str("mkdir", 5),
		mp_obj_new_str(path, strlen(path)),
		mp_obj_new_int(mode),
	};
	return mp_obj_get_int(mp_call_function_n_kw(s_callback, 3, 0, &args[0]));
}

