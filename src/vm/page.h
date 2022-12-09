#include <hash.h>
#include "filesys/off_t.h"

#define MAX_STACK 0x800000

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
  bool from_mapped_file;

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
bool install_mapped_file_entry_in_spt (struct hash *spt, void *upage, struct file *file, off_t offset, uint32_t page_read_bytes, uint32_t page_zero_bytes, bool writable);
bool has_entry_in_spt (struct hash *spt, void *upage);
struct supplemental_page_table_entry * get_entry_in_spt (struct hash *spt, void *upage);
bool load_page_from_spt (struct hash *spt, void *upage, uint32_t *pagedir, bool pinned);
void unpin_page (struct hash *spt, void *upage);
bool install_frame_entry_in_spt (struct hash *spt, void *upage, void *kpage, bool writable);
bool install_allzero_entry_in_spt (struct hash *spt, void *upage);
bool load_page_on_allzero(struct supplemental_page_table_entry *spte, void *upage, uint32_t *pagedir);
void spt_unmap (struct hash *spt, void *upage, uint32_t *pagedir, off_t offset, int size);

