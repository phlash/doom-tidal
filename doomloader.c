// A loader that tinkers with MMU to map in doom.bin, fiddles with realloc() to reserve heap memory
// then copies in initialised data and finally calls the entry point!
#include <py/dynruntime.h>

// @see <esp-idf>/components/soc/include/esp32s3/soc/cache_memory.h
#define MMUMAPBASE		0x600C5000
#define MMUMAXPAGE		0x200
#define MMUPAGE(slot)	*((volatile uint32_t *)MMUMAPBASE+slot)
#define MMUPAGEINVAL	0x4000
#define MMUPAGESIZE		0x10000
#define MMUPAGEMASK		0x0ffff
#define MMUIROMBASE		0x42000000
#define MMUDROMBASE		0x3C000000
#define MMUMAXREGION	0x02000000
#define MMUIROMTOP		(MMUIROMBASE+MMUMAXREGION)
#define MMUDROMTOP		(MMUDROMBASE+MMUMAXREGION)

// @see memstuff/uPython.map
#define INIT_ALLOC		0x700000
#define FINAL_ALLOC		0x040000
#define DATABASE		0x3D900000

#define OTA0BASE		0x10000
#define OTA1BASE		0x210000
#define OTASIZE			0x200000	// 2MiB
#define TEMPDROMADDR	0x3C400000	// 4MiB into address space

#pragma pack(push,1)
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
#pragma pack(pop)

// memory mapping magic :=)
static int domap(uint32_t offset, uint32_t len, uint32_t vaddr, bool unmap) {
	// sanity check we are on page boundary
	if (offset & MMUPAGEMASK || vaddr & MMUPAGEMASK) {
		mp_printf(&mp_plat_print, "m[un]map of non-aligned offset/vaddr: %p/%p\n", offset, vaddr);
		return -1;
	}
	// convert vaddr to offset in appropriate address space
	uint32_t voff = 0;
	if (vaddr >= MMUIROMBASE && vaddr < MMUIROMTOP)
		voff = vaddr-MMUIROMBASE;
	else if (vaddr >= MMUDROMBASE && vaddr < MMUDROMTOP)
		voff = vaddr-MMUDROMBASE;
	else {
		mp_printf(&mp_plat_print, "vaddr in unknown address region: %p\n", vaddr);
		return -2;
	}
	// sanity check we don't exceed address space
	uint32_t pages = (len+MMUPAGESIZE-1)/MMUPAGESIZE;
	if ((voff + pages*MMUPAGESIZE) > MMUMAXREGION) {
		mp_printf(&mp_plat_print, "vaddr+pages exceeds region size: %p/%u\n", vaddr, pages);
		return -3;
	}
	uint32_t page = offset/MMUPAGESIZE;
	uint32_t slot = voff/MMUPAGESIZE;
	mp_printf(&mp_plat_print, "m[un]map: %p(page %x) => %p(slot %x) %x (%x pages) unmap=%d\n",
		offset, page, vaddr, slot, len, pages, unmap);
	// Do it!
	for (uint32_t p=0; p<pages; p++, page++, slot++)
		MMUPAGE(slot) = unmap ? MMUPAGEINVAL : page;
	return 0;
}
static int mmap(uint32_t offset, uint32_t len, uint32_t vaddr) {
	return domap(offset, len, vaddr, false);
}
static int munmap(uint32_t len, uint32_t vaddr) {
	return domap(0, len, vaddr, true);
}

// our uPython callable function to run doom..
STATIC mp_obj_t doom(mp_obj_t oobj, mp_obj_t callback) {
	char *err = NULL;
	int rv = 0;
	#define _ERR(m, r)	{ err = m; rv = r; goto done; }
	int ota = mp_obj_get_int(oobj);
	esp_image_header_t *hdr;
	esp_image_segment_t *seg;
	// allocate RAM, check address is suitable then realloc to sensible size
	void *ptr = m_malloc(INIT_ALLOC);
	if (!ptr) {
		// try again 1MiB less
		ptr = m_malloc(INIT_ALLOC-0x100000);
		if (!ptr)
			_ERR("oops: not enough free memory", -1);
	}
	if ((uint32_t)ptr > DATABASE)
		_ERR("oops: too much memory already allocated", -2);
	uint32_t resize = DATABASE - (uint32_t)ptr + FINAL_ALLOC;
	ptr = mp_fun_table.realloc_(ptr, resize, false);
	if (!ptr)
		_ERR("oops: realloc failed", -3);
	mp_printf(&mp_plat_print, ".data segment allocated: %p-%p\n", ptr, (uint8_t*)ptr+resize);
	// mmap entire OTA partition so we can read segment info
	// use page well out of the way..
	uint32_t otabase = ota>0 ? OTA1BASE : OTA0BASE;
	if (mmap(otabase, OTASIZE, TEMPDROMADDR)<0)
		_ERR("mmap failed", -4);
	uint32_t otaoff = TEMPDROMADDR;
	hdr = (void *)otaoff;
	otaoff += sizeof(*hdr);
	mp_printf(&mp_plat_print,
		"doom.bin:\n" \
		"\tchip: %x\n" \
		"\tsegs: %d\n" \
		"\tentry: %x\n",
		hdr->chip, hdr->segs, hdr->entry);
	for (uint32_t s=0; s<hdr->segs; s++) {
		seg = (void *)otaoff;
		otaoff += sizeof(*seg);
		mp_printf(&mp_plat_print,
			"seg:%d\n" \
			"\toffs: %x\n" \
			"\tload: %x\n" \
			"\tsize: %x\n",
			s, otaoff-TEMPDROMADDR, seg->load, seg->size);
		// round down offset and load to page boundary, adjust size
		uint32_t moffs = (otaoff - TEMPDROMADDR + otabase) & ~(MMUPAGEMASK);
		uint32_t mload = seg->load & ~(MMUPAGEMASK);
		uint32_t msize = seg->size + (seg->load - mload);
		// map it (unless padding)
		if (seg->load && mmap(moffs, msize, mload)<0)
			_ERR("mmap failed", -4);
		otaoff += seg->size;
	}
	if (munmap(OTASIZE, TEMPDROMADDR)<0)
		_ERR("munmap failed", -5);
	// in theory, we're ready to roll..
	mp_printf(&mp_plat_print, "calling run_doom()@%x\n", hdr->entry);
	int (*run_doom)(mp_obj_t, const mp_fun_table_t *) = (void *)hdr->entry;
	rv = run_doom(callback, &mp_fun_table);
done:
	if (err)
		mp_printf(&mp_plat_print, "%s\n", err);
	if (ptr)
		m_free(ptr);
	return mp_obj_new_int(rv);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(doom_obj, doom);

mp_obj_t mpy_init(mp_obj_fun_bc_t *self, size_t n_args, size_t n_kw, mp_obj_t *args) {
	MP_DYNRUNTIME_INIT_ENTRY;
	mp_store_global(MP_QSTR_doom, MP_OBJ_FROM_PTR(&doom_obj));
	MP_DYNRUNTIME_INIT_EXIT;
}
