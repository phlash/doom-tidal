// A loader that tinkers with MMU to map in doom.bin, allocates a huge ram block and calls
// mapped binary..
#include <py/dynruntime.h>

// @see <esp-idf>/components/soc/include/esp32s3/soc/cache_memory.h
#define MMUMAPBASE		0x600C5000
#define MMUMAXPAGE		0x800
#define MMUGPAGE(page)	*((volatile uint32_t *)MMUMAPBASE+page)

// @see memstuff/uPython.map
#define INIT_ALLOC		0x700000
#define FINAL_ALLOC		0x100000
#define RODATABASE		0x3C000000
#define DATABASE		0x3D900000
#define TEXTBASE		0x42200000

typedef struct {
	uint8_t chip;
	uint8_t segs;
	uint8_t ftyp;
	uint8_t fsiz;
	uint32_t entry;
	uint8_t pad[16];
} esp_image_header_t;
typedef struct {
	uint32_t load;
	uint32_t size;
} esp_image_segment_t;

mp_obj_t dl_callback;

// roll your own stdio via uPython helper
static size_t strlen(const char *s) {
	size_t i = 0;
	while (s[i]) i++;
	return i;
}
struct _fileio;
typedef struct _fileio FILE;
static FILE *fopen(const char *path, const char *mode) {
	mp_obj_t args[3] = {
		mp_obj_new_str("fopen", 5),
		mp_obj_new_str(path, strlen(path)),
		mp_obj_new_str(mode, strlen(mode)),
	};
	return MP_OBJ_TO_PTR(mp_call_function_n_kw(dl_callback, 3, 0, &args[0]));
}
static void fclose(FILE *fp) {
	mp_obj_t args[2] = {
		mp_obj_new_str("fclose", 6),
		MP_OBJ_FROM_PTR(fp),
	};
	mp_call_function_n_kw(dl_callback, 2, 0, &args[0]);
}
static size_t fread(void *buf, size_t len, size_t cnt, FILE *fp) {
	mp_obj_t args[3] = {
		mp_obj_new_str("fread", 5),
		mp_obj_new_bytearray_by_ref(len*cnt, buf),
		MP_OBJ_FROM_PTR(fp),
	};
	return mp_obj_get_int(mp_call_function_n_kw(dl_callback, 4, 0, &args[0]))/len;
}
static int fseek(FILE *fp, long off, int w) {
	mp_obj_t args[4] = {
		mp_obj_new_str("fseek", 5),
		MP_OBJ_FROM_PTR(fp),
		mp_obj_new_int(off),
		mp_obj_new_int(w),
	};
	return mp_obj_get_int(mp_call_function_n_kw(dl_callback, 4, 0, &args[0]));
}

// our uPython callable function to run doom..
STATIC mp_obj_t doom(mp_obj_t callback) {
	char *err = NULL;
	int rv = 0;
	#define _ERR(m, r)	{ err = m; rv = r; goto done; }
	esp_image_header_t hdr;
	dl_callback = callback;
	// allocate RAM, check address is suitable then realloc to sensible size
	void *ptr = m_malloc(INIT_ALLOC);
	if (!ptr)
		_ERR("oops: not enough free memory", -1);
	if ((uint32_t)ptr > DATABASE)
		_ERR("oops: too much memory already allocated", -2);
	uint32_t resize = DATABASE - (uint32_t)ptr + FINAL_ALLOC;
	ptr = mp_fun_table.realloc_(ptr, resize, false);
	if (!ptr)
		_ERR("oops: realloc failed", -3);
	mp_printf(&mp_plat_print, ".data segment allocated: %p-%p\n", ptr, (uint8_t*)ptr+resize);
	// open doom.bin, read segments and map / load them
	FILE *dp = fopen("doom.bin", "rb");
	if (!dp)
		_ERR("oops: unable to open doom.bin", -4);
	if (fread(&hdr, sizeof(hdr), 1, dp) != 1)
		_ERR("oops: unable to read header", -5);
	mp_printf(&mp_plat_print,
		"doom.bin:\n" \
		"\tchip: %x\n" \
		"\tsegs: %d\n",
		hdr.chip, hdr.segs);
	for (int s=0; s<hdr.segs; s++) {
		esp_image_segment_t seg;
		if (fread(&seg, sizeof(seg), 1, dp) != 1)
			_ERR("oops: unable to read segment info", -6);
		if (fseek(dp, seg.size, MP_SEEK_CUR)<0)
			_ERR("oops: unable to seek past segment", -7);
		mp_printf(&mp_plat_print, "segment: %d\n" \
			"\tload: %x\n" \
			"\tsize: %x\n",
			s, seg.load, seg.size);
	}
	fclose(dp);
done:
	if (err)
		mp_printf(&mp_plat_print, "%s\n", err);
	if (ptr)
		m_free(ptr);
	return mp_obj_new_int(rv);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(doom_obj, doom);

mp_obj_t mpy_init(mp_obj_fun_bc_t *self, size_t n_args, size_t n_kw, mp_obj_t *args) {
	MP_DYNRUNTIME_INIT_ENTRY;
	mp_store_global(MP_QSTR_doom, MP_OBJ_FROM_PTR(&doom_obj));
	MP_DYNRUNTIME_INIT_EXIT;
}
