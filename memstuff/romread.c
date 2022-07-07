#include <py/dynruntime.h>

// @see <esp-idf>/components/soc/include/esp32s3/soc/cache_memory.h
#define MMUMAPBASE		0x600C5000
#define MMUMAXPAGE		0x200
#define MMUINVALIDPAGE	0x4000
#define MMUPAGE(page)	*((volatile uint32_t *)MMUMAPBASE+page)

typedef uint32_t (*rom_function_t)();
#define ESPROM_CACHE_INVALIDATE_ICACHE	0x400016d4
#define ESPROM_CACHE_INVALIDATE_DCACHE	0x400016e0
#define ESPROM_CACHE_SUSPEND_ICACHE		0x4000189c
#define ESPROM_CACHE_RESUME_ICACHE		0x400018a8
#define ESPROM_CACHE_SUSPEND_DCACHE		0x400018b4
#define ESPROM_CACHE_RESUME_DCACHE		0x400018c0
#define ESPROM_CACHE_GET_IROM_MMU_END	0x4000195c
#define ESPROM_CACHE_GET_DROM_MMU_END	0x40001968

STATIC mp_obj_t go(mp_obj_t lobj) {
	uint32_t len = mp_obj_get_int(lobj);
	mp_printf(&mp_plat_print, "dumping MMU pages (%u)..\n");
	uint32_t irom_end = ((rom_function_t)ESPROM_CACHE_GET_IROM_MMU_END)()/4;
	uint32_t drom_end = ((rom_function_t)ESPROM_CACHE_GET_DROM_MMU_END)()/4;
	mp_printf(&mp_plat_print, "0 <-- irom --> %x <-- drom --> %x\n", irom_end, drom_end);
	uint32_t page, val;
	for (page=0; page<MMUMAXPAGE && page<len; page++) {
		val = MMUPAGE(page);
		mp_printf(&mp_plat_print, "%x: %x\n", page, val);
	}
	mp_printf(&mp_plat_print, "done\n");
	return mp_obj_new_int(0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(go_obj, go);

static mp_obj_t domap(mp_obj_t pobj, mp_obj_t vobj, mp_obj_t cobj, bool unmp) {
	uint32_t phys = mp_obj_get_int(pobj);
	uint32_t virt = mp_obj_get_int(vobj);
	uint32_t cnt  = mp_obj_get_int(cobj);
	// choose appropriate rom function from virt address range
	bool icache = false;
	// if I'm in MMU space...??
	if (virt>=0x42000000 && virt<0x44000000) {
		mp_printf(&mp_plat_print, "mapping %08x -> %08x (%u), unmap=%d, icache=%d\n",
			phys, virt, cnt, unmp, icache);
		//((rom_function_t)ESPROM_CACHE_INVALIDATE_ICACHE)();
		virt -= 0x42000000;
		icache = true;
	} else if (virt>=0x3C000000 && virt<0x3E000000) {
		mp_printf(&mp_plat_print, "mapping %08x -> %08x (%u), unmap=%d, icache=%d\n",
			phys, virt, cnt, unmp, icache);
		//((rom_function_t)ESPROM_CACHE_INVALIDATE_DCACHE)();
		virt -= 0x3C000000;
	} else {
		mp_printf(&mp_plat_print, "virt out of range - try again!\n");
		return mp_obj_new_int(1);
	}
	// not gonna check anything - trust the hacker ;)
	uint32_t page = unmp ? MMUINVALIDPAGE : phys>>16;
	uint32_t slot = virt>>16;
	while (cnt--) {
		MMUPAGE(slot) = page;
		if (!unmp)
			page += 1;
		slot += 1;
	}
	mp_printf(&mp_plat_print, "done!\n");
	return mp_obj_new_int(0);
}
STATIC mp_obj_t mmap(mp_obj_t pobj, mp_obj_t vobj, mp_obj_t cobj) {
	return domap(pobj, vobj, cobj, false);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(mmap_obj, mmap);
STATIC mp_obj_t munmap(mp_obj_t pobj, mp_obj_t vobj, mp_obj_t cobj) {
	return domap(pobj, vobj, cobj, true);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(munmap_obj, munmap);

#define RDITLB(at, as)        __asm__ volatile ("ritlb1  %0, %1; \n" : "=r" (at) : "r" (as))
#define RDDTLB(at, as)        __asm__ volatile ("rdtlb1  %0, %1; \n" : "=r" (at) : "r" (as))
STATIC mp_obj_t tlbs(void) {
	mp_printf(&mp_plat_print, "dumping TLB entries..\n");
	mp_printf(&mp_plat_print, "region   ia da\n");
	uint32_t e;
	for (e=0; e<8; e++) {
		uint32_t addr = e*0x20000000;
		uint32_t iattr = 0, dattr = 0;
		RDITLB(iattr, addr);
		RDDTLB(dattr, addr);
		mp_printf(&mp_plat_print, "%08x %02x %02x\n", addr, iattr&0xf, dattr&0xf);
	}
	return mp_obj_new_int(0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(tlbs_obj, tlbs);

// @see 'esp32-s3_technical_reference_manaual_en.pdf' $13 PMS and..
// <esp-idf>/components/soc/include/esp32s3/soc/sensitive_regs.h
#define SENSBASE		0x600C1000
#define PMSIRAM0LOCK	*((uint32_t *)(SENSBASE+0xD8))
#define PMSIRAM0BITS1	*((uint32_t *)(SENSBASE+0xDC))
#define PMSIRAM0BITS2	*((uint32_t *)(SENSBASE+0xE0))
#define PMSDRAM0LOCK	*((uint32_t *)(SENSBASE+0xFC))
#define PMSDRAM0BITS1	*((uint32_t *)(SENSBASE+0x100))
#define PMSC0VECBASEO	*((uint32_t *)(SENSBASE+0x1C0))
#define PMSC0VECBASE2	*((uint32_t *)(SENSBASE+0x1C4))
#define APBCTLBASE		0x60026000
#define APBFLREGIONMAX	4
#define APBCTLFLPMS(n)	*((uint32_t *)(APBCTLBASE+0x28+(n)*4))
#define APBCTLFLADR(n)	*((uint32_t *)(APBCTLBASE+0x38+(n)*4))
#define APBCTLFLLEN(n)	*((uint32_t *)(APBCTLBASE+0x48+(n)*4))
STATIC mp_obj_t pms(void) {
	mp_printf(&mp_plat_print, "dumping PMS bitfields..\n");
	uint32_t val = PMSIRAM0LOCK;
	mp_printf(&mp_plat_print, "ilock: %08x\n", val);
	val = PMSIRAM0BITS1;
	mp_printf(&mp_plat_print, "ibits1: %08x\n", val);
	val = PMSIRAM0BITS2;
	mp_printf(&mp_plat_print, "ibits2: %08x\n", val);
	val = PMSDRAM0LOCK;
	mp_printf(&mp_plat_print, "dlock: %08x\n", val);
	val = PMSDRAM0BITS1;
	mp_printf(&mp_plat_print, "dbits1: %08x\n", val);
	val = PMSC0VECBASEO;
	mp_printf(&mp_plat_print, "c0veco: %08x\n", val);
	val = PMSC0VECBASE2;
	mp_printf(&mp_plat_print, "c0vec2: %08x\n", val);
	for (uint32_t r=0; r<APBFLREGIONMAX; r++) {
		val = APBCTLFLPMS(r);
		mp_printf(&mp_plat_print, "fl%u: pms:  %08x\n", r, val);
		val = APBCTLFLADR(r);
		mp_printf(&mp_plat_print, "fl%u: addr: %08x\n", r, val);
		val = APBCTLFLLEN(r);
		mp_printf(&mp_plat_print, "fl%u: len:  %08x\n", r, val);
	}
	return mp_obj_new_int(0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(pms_obj, pms);

STATIC mp_obj_t grab(mp_obj_t aobj, mp_obj_t sobj) {
	uint32_t addr = mp_obj_get_int(aobj);
	uint32_t size = mp_obj_get_int(sobj);
	mp_printf(&mp_plat_print, "grabbing: %x len: %x\n", addr, size);
	void *buf = m_malloc(size);
	if (!buf) {
		mp_printf(&mp_plat_print, "out of memory :(\n");
		return mp_obj_new_int(0);
	}
	// for instruction address space, we MUST use 32bit access
	if (addr>=0x42000000 && addr<0x44000000) {
		uint32_t *uib = (uint32_t *)buf;
		uint32_t *uip = (uint32_t *)(addr & ~0x3);
		uint32_t cnt = size / 4;
		for (uint32_t i=0; i<cnt; i++)
			uib[i] = uip[i];
	} else {
		mp_fun_table.memmove_(buf, (void *)addr, size);
	}
	return mp_obj_new_bytearray_by_ref(size, buf);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(grab_obj, grab);

STATIC mp_obj_t yank(mp_obj_t sobj) {
	uint32_t size = mp_obj_get_int(sobj);
	void *ptr = m_malloc(size);
	mp_printf(&mp_plat_print, "malloc(%u)=%p -> %p\n", size, ptr, (uint8_t*)ptr+size);
	return MP_OBJ_FROM_PTR(ptr);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(yank_obj, yank);

STATIC mp_obj_t size(mp_obj_t pobj, mp_obj_t sobj) {
	void *ptr = MP_OBJ_TO_PTR(pobj);
	uint32_t siz = mp_obj_get_int(sobj);
	void *rv = m_realloc(ptr, siz);
	mp_printf(&mp_plat_print, "realloc(%p,%u)=%p -> %p\n", ptr, siz, rv, (uint8_t*)rv+siz);
	return MP_OBJ_FROM_PTR(rv);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(size_obj, size);

STATIC mp_obj_t drop(mp_obj_t pobj) {
	void *ptr = MP_OBJ_TO_PTR(pobj);
	mp_printf(&mp_plat_print, "free(%p)\n", ptr);
	if (ptr)
		m_free(ptr);
	return mp_obj_new_int(0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(drop_obj, drop);

mp_obj_t mpy_init(mp_obj_fun_bc_t *self, size_t n_args, size_t n_kw, mp_obj_t *args) {
	MP_DYNRUNTIME_INIT_ENTRY;
	mp_store_global(MP_QSTR_go, MP_OBJ_FROM_PTR(&go_obj));
	mp_store_global(MP_QSTR_mmap, MP_OBJ_FROM_PTR(&mmap_obj));
	mp_store_global(MP_QSTR_munmap, MP_OBJ_FROM_PTR(&munmap_obj));
	mp_store_global(MP_QSTR_tlbs, MP_OBJ_FROM_PTR(&tlbs_obj));
	mp_store_global(MP_QSTR_pms, MP_OBJ_FROM_PTR(&pms_obj));
	mp_store_global(MP_QSTR_grab, MP_OBJ_FROM_PTR(&grab_obj));
	mp_store_global(MP_QSTR_size, MP_OBJ_FROM_PTR(&size_obj));
	mp_store_global(MP_QSTR_yank, MP_OBJ_FROM_PTR(&yank_obj));
	mp_store_global(MP_QSTR_drop, MP_OBJ_FROM_PTR(&drop_obj));
	MP_DYNRUNTIME_INIT_EXIT;
}
