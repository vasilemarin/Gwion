#ifndef __IMPORT_ITEM
#define __IMPORT_ITEM

ANN m_int gwi_item_ini(const Gwi gwi, const m_str type, const m_str name);
ANN2(1) m_int gwi_item_end(const Gwi gwi, const ae_flag flag, union value_data);
#define gwi_item_end(a, b, c) gwi_item_end(a, (const ae_flag)(b), c)
ANN void ck_clean_item(MemPool, ImportCK*);
#endif
