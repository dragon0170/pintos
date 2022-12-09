#include <stddef.h>

void swap_init (void);
void swap_destroy (void);
void swap_in (size_t index, void *addr);
size_t swap_out (void *addr);
void swap_free (size_t index);
