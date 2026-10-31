#include "memmap.h"
#include "fdt.h"

void memmap_init(memmap_map_t* map);

void memmap_add_ram(memmap_map_t* map);
void memmap_reserve(memmap_map_t* map);
void memmap_finalize(memmap_map_t* map);
void memmap_next_usable();
