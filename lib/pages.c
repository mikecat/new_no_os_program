#include "pusha_regs.h"

void entry(struct pusha_regs* regs);

void initialize_pages(struct pusha_regs* regs) {
	/* TODO */
	entry(regs);
	for(;;);
}
