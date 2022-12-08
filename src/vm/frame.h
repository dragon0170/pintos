#include <hash.h>
#include "threads/thread.h"
#include "threads/palloc.h"

struct frame_table_entry
{
  void *kpage;
  void *upage;
  struct thread *owner;
  bool pinned;

  struct hash_elem elem;
};

void frame_init (void);
void * allocate_frame (enum palloc_flags flags, void *upage);
void free_frame (void *kpage);
void free_frame_without_free_page (void *kpage);
void unpin_frame (void *kpage);
void pin_frame (void *kpage);
