#include <hash.h>
#include "filesys/off_t.h"

enum page_state {
  ON_FRAME,
  ON_FILESYS,
  SWAPPED_OUT,
  ALL_ZERO,
};

struct supplemental_page_table_entry
{
  void *upage;
  void *kpage;

  enum page_state state;
  bool dirty;

  struct hash_elem elem;

  size_t swap_index;

  struct file *file;
  off_t file_offset;
  uint32_t file_read_bytes, file_zero_bytes;
  bool writable;
};

struct hash * create_spt (void);
void destroy_spt (struct hash *spt);
bool install_filesys_entry_in_spt (struct hash *spt, void *upage, struct file *file, off_t offset, uint32_t page_read_bytes, uint32_t page_zero_bytes, bool writable);
bool has_entry_in_spt (struct hash *spt, void *upage);
bool load_page_from_spt (struct hash *spt, void *upage, uint32_t *pagedir);
bool install_frame_entry_in_spt (struct hash *spt, void *upage, void *kpage, bool writable);
void spt_unmap (struct hash *spt, void *upage, uint32_t *pagedir, off_t offset, int size);
