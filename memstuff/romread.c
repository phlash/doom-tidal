#include <py/dynruntime.h>

// @see <esp-idf>/components/soc/include/esp32s3/soc/cache_memory.h
#define MMUMAPBASE		0x600C5000
#define MMUMAXPAGE		0x800
#define MMUGPAGE(page)	*((volatile uint32_t *)MMUMAPBASE+page)

STATIC mp_obj_t go(void) {
	mp_printf(&mp_plat_print, "dumping MMU pages..\n");
	uint32_t irom_end = 0x100;
	uint32_t drom_end = 0x200;
	mp_printf(&mp_plat_print, "0 <-- irom --> %x <-- drom --> %x\n", irom_end, drom_end);
	uint32_t page, val;
	for (page=0; page<drom_end; page++) {
		val = MMUGPAGE(page);
		mp_printf(&mp_plat_print, "%x: %x\n", page, val);
	}
	mp_printf(&mp_plat_print, "done\n");
	return mp_obj_new_int(0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(go_obj, go);

STATIC mp_obj_t grab(mp_obj_t aobj, mp_obj_t sobj) {
	uint32_t addr = mp_obj_get_int(aobj);
	uint32_t size = mp_obj_get_int(sobj);
	mp_printf(&mp_plat_print, "grabbing: %x len: %x\n", addr, size);
	void *buf = m_malloc(size);
	if (!buf) {
		mp_printf(&mp_plat_print, "out of memory :(\n");
		return mp_obj_new_int(0);
	}
	mp_fun_table.memmove_(buf, (void *)addr, size);
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
	mp_store_global(MP_QSTR_grab, MP_OBJ_FROM_PTR(&grab_obj));
	mp_store_global(MP_QSTR_size, MP_OBJ_FROM_PTR(&size_obj));
	mp_store_global(MP_QSTR_yank, MP_OBJ_FROM_PTR(&yank_obj));
	mp_store_global(MP_QSTR_drop, MP_OBJ_FROM_PTR(&drop_obj));
	MP_DYNRUNTIME_INIT_EXIT;
}
