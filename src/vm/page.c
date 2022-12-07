#include <hash.h>
#include <string.h>
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "filesys/off_t.h"
#include "filesys/file.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "threads/vaddr.h"
#include "userprog/process.h"

static unsigned
spt_hash_func (const struct hash_elem *elem, void *aux UNUSED)
{
  struct supplemental_page_table_entry *entry = hash_entry (elem, struct supplemental_page_table_entry, elem);
  return hash_int ((int) entry->upage);
}
static bool
spt_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  struct supplemental_page_table_entry *a_entry = hash_entry (a, struct supplemental_page_table_entry, elem);
  struct supplemental_page_table_entry *b_entry = hash_entry (b, struct supplemental_page_table_entry, elem);
  return a_entry->upage < b_entry->upage;
}

static void
spt_destroy_func (struct hash_elem *elem, void *aux UNUSED)
{
  struct supplemental_page_table_entry *entry = hash_entry (elem, struct supplemental_page_table_entry, elem);

  if (entry->state == ON_FRAME)
    {
      ASSERT (entry->kpage != NULL);
      free_frame_without_free_page (entry->kpage);
    }
  else if (entry->state == SWAPPED_OUT)
    {
      // TODO: call function for freeing swap by swap_index
    }

  free (entry);
}

struct hash *
create_spt (void)
{
  struct hash *spt = (struct hash *) malloc (sizeof (struct hash));
  hash_init (spt, spt_hash_func, spt_less_func, NULL);
  return spt;
}

void
destroy_spt (struct hash *spt)
{
  ASSERT (spt != NULL);
  hash_destroy (spt, spt_destroy_func);
  free (spt);
}

bool
install_filesys_entry_in_spt (struct hash *spt, void *upage, struct file *file, off_t offset,
                              uint32_t page_read_bytes, uint32_t page_zero_bytes, bool writable)
{
  ASSERT (spt != NULL);
  ASSERT (upage != NULL);
  ASSERT (file != NULL);

  struct supplemental_page_table_entry *spte = malloc (sizeof (struct supplemental_page_table_entry));
  if (spte == NULL)
    return false;

  spte->upage = upage;
  spte->kpage = NULL;
  spte->state = ON_FILESYS;
  spte->dirty = false;
  spte->file = file;
  spte->file_offset = offset;
  spte->file_read_bytes = page_read_bytes;
  spte->file_zero_bytes = page_zero_bytes;
  spte->writable = writable;

  hash_insert (spt, &spte->elem);

  return true;
}

bool
install_frame_entry_in_spt (struct hash *spt, void *upage, void *kpage, bool writable)
{
  ASSERT (spt != NULL);
  ASSERT (upage != NULL);
  ASSERT (kpage != NULL);

  struct supplemental_page_table_entry *spte = malloc (sizeof (struct supplemental_page_table_entry));
  if (spte == NULL)
    return false;

  spte->upage = upage;
  spte->kpage = kpage;
  spte->state = ON_FRAME;
  spte->dirty = false;
  spte->writable = writable;

  hash_insert (spt, &spte->elem);

  return true;
}

bool
install_allzero_entry_in_spt (struct hash *spt, void *upage)
{
  ASSERT (spt != NULL);
  ASSERT (upage != NULL);

  struct supplemental_page_table_entry *spte = malloc (sizeof (struct supplemental_page_table_entry));
  if (spte == NULL)
    return false;

  spte->upage = upage;
  spte->kpage = NULL;
  spte->state = ALL_ZERO;
  spte->dirty = false;

  if(hash_insert (spt, &spte->elem) == NULL)
    return true;

  return false;
}

static struct supplemental_page_table_entry *
get_entry_in_spt (struct hash *spt, void *upage)
{
  ASSERT (spt != NULL);
  ASSERT (upage != NULL);

  struct supplemental_page_table_entry entry_for_find;
  entry_for_find.upage = upage;

  struct hash_elem *h_elem = hash_find (spt, &entry_for_find.elem);
  if (h_elem == NULL)
    return NULL;
  return hash_entry (h_elem, struct supplemental_page_table_entry, elem);
}

bool
has_entry_in_spt (struct hash *spt, void *upage)
{
  ASSERT (spt != NULL);

  struct supplemental_page_table_entry entry_for_find;
  entry_for_find.upage = upage;

  struct hash_elem *h_elem = hash_find (spt, &entry_for_find.elem);
  return h_elem != NULL;
}

static bool
load_page_on_filesys (struct supplemental_page_table_entry* spte, uint32_t *pagedir)
{
  /* Get a page of memory. */
  uint8_t *kpage = allocate_frame (PAL_USER, spte->upage);
  if (kpage == NULL)
    return false;
  spte->kpage = kpage;

  file_seek (spte->file, spte->file_offset);
  /* Load this page. */
  if (file_read (spte->file, spte->kpage, spte->file_read_bytes) != (int) spte->file_read_bytes)
    {
      free_frame (spte->kpage);
      return false;
    }
  memset (spte->kpage + spte->file_read_bytes, 0, spte->file_zero_bytes);

  /* Add the page to the process's address space. */
  if (pagedir_get_page (pagedir, spte->upage) != NULL
      || !pagedir_set_page (pagedir, spte->upage, spte->kpage, spte->writable))
    {
      free_frame (spte->kpage);
      return false;
    }

  spte->state = ON_FRAME;
  return true;
}

bool
load_page_on_allzero(struct supplemental_page_table_entry *spte, void *upage, uint32_t *pagedir)
{
  void *kpage = allocate_frame(PAL_USER, upage);

  if(kpage == NULL)
    return false;

  memset(kpage, 0, PGSIZE);

  if(!pagedir_set_page(pagedir, upage, kpage, true))
  {
    free_frame(kpage);
    return false;
  }

  spte->kpage = kpage;
  spte->state = ON_FRAME;

  pagedir_set_dirty(pagedir, kpage, false);

  return true;
}

bool
load_page_from_spt (struct hash *spt, void *upage, uint32_t *pagedir)
{
  ASSERT (spt != NULL);
  ASSERT (upage != NULL);
  ASSERT (pagedir != NULL);

  struct supplemental_page_table_entry *spte = get_entry_in_spt (spt, upage);
  if (spte == NULL)
    return false;

  bool result = false;
  switch (spte->state)
    {
    case ON_FRAME:
      break;
    case ON_FILESYS:
      result = load_page_on_filesys (spte, pagedir);
      break;
    case SWAPPED_OUT:
      break;
    case ALL_ZERO:
      result = load_page_on_allzero(spte, upage, pagedir);
      break;
    default:
      break;
    }

  if (result)
    pagedir_set_dirty (pagedir, spte->upage, false);

  return result;
}

void
spt_unmap (struct hash *spt, void *upage, uint32_t *pagedir, off_t offset, int size)
{
  struct supplemental_page_table_entry *spte = get_entry_in_spt (spt, upage);
  if (spte == NULL)
    return;

  switch (spte->state)
    {
      case ON_FRAME:
        ASSERT (spte->kpage != NULL);
        if (spte->dirty || pagedir_is_dirty (pagedir, spte->upage))
          file_write_at (spte->file, upage, size, offset);
        free_frame (spte->kpage);
        pagedir_clear_page (pagedir, upage);
        break;
      case ON_FILESYS:
        break;
      default:
        PANIC ("you should not be here");
        break;
    }
  hash_delete (spt, &spte->elem);
}

