#ifndef MTRR_H
#define MTRR_H

#include <include/types.h>

/*
 * Mark a physical address range Write Combining using free
 * variable-range MTRRs.
 *
 * Returns true on success.
 */
bool mtrr_set_write_combining(uint64_t phys_addr, uint64_t length);
void mtrr_dump(void);
#endif
