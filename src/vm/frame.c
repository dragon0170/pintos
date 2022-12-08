#include <stdio.h>
#include <hash.h>
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"

static struct lock frame_table_lock;
static struct hash frame_table;

static void internal_free_frame_with_lock_held (void *kpage, bool should_free_page);

static unsigned frame_hash_func(const struct hash_elem *h_elem, void *aux UNUSED)
{
  struct frame_table_entry *entry = hash_entry (h_elem, struct frame_table_entry, elem);
  return hash_bytes (&entry->kpage, sizeof entry->kpage);
}
static bool frame_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  struct frame_table_entry *a_entry = hash_entry (a, struct frame_table_entry, elem);
  struct frame_table_entry *b_entry = hash_entry (b, struct frame_table_entry, elem);
  return a_entry->kpage < b_entry->kpage;
}

void
frame_init (void)
{
  lock_init (&frame_table_lock);
  hash_init (&frame_table, frame_hash_func, frame_less_func, NULL);
}

static struct frame_table_entry *
select_victim_for_eviction ()
{
  struct hash_iterator iter;
  hash_first (&iter, &frame_table);

  while (hash_next (&iter))
    {
      struct frame_table_entry *entry = hash_entry (hash_cur (&iter), struct frame_table_entry, elem);
      if (entry->pinned)
        continue;
      if (pagedir_is_accessed (entry->owner->pagedir, entry->upage))
        pagedir_set_accessed (entry->owner->pagedir, entry->upage, false);
      else
        return entry;
    }

  hash_first (&iter, &frame_table);

  while (hash_next (&iter))
    {
      struct frame_table_entry *entry = hash_entry(hash_cur (&iter), struct frame_table_entry, elem);
      if (entry->pinned)
        continue;
      if (pagedir_is_accessed (entry->owner->pagedir, entry->upage))
        pagedir_set_accessed (entry->owner->pagedir, entry->upage, false);
      else
        return entry;
    }

  PANIC("No victim for eviction");
}

void *
allocate_frame (enum palloc_flags flags, void *upage)
{
  ASSERT (is_user_vaddr (upage));

  lock_acquire (&frame_table_lock);

  void *kpage = palloc_get_page (flags);
  if (kpage == NULL)
    {
      struct frame_table_entry *victim_frame = select_victim_for_eviction ();
      ASSERT (victim_frame != NULL);
      struct supplemental_page_table_entry *spte = get_entry_in_spt(victim_frame->owner->spt, victim_frame->upage);
      ASSERT (spte != NULL);

      if (spte->file != NULL && spte->from_mapped_file)
        {
          // NO SWAP - frame from memory mapped file
          if (pagedir_is_dirty (victim_frame->owner->pagedir, victim_frame->upage) && spte->writable)
            file_write_at (spte->file, victim_frame->kpage, spte->file_read_bytes, spte->file_offset);
          spte->kpage = NULL;
          spte->state = ON_FILESYS;
        }
      else if (spte->file != NULL && !spte->writable)
        {
          // NO SWAP - frame from readonly executable file
          spte->kpage = NULL;
          spte->state = ON_FILESYS;
        }
      else
        {
          // SWAP
          size_t swap_index = swap_out (victim_frame->kpage);
          spte->kpage = NULL;
          spte->state = SWAPPED_OUT;
          spte->swap_index = swap_index;
        }
      pagedir_clear_page (victim_frame->owner->pagedir, victim_frame->upage);
      internal_free_frame_with_lock_held (victim_frame->kpage, true);

      kpage = palloc_get_page (flags);
      ASSERT (kpage != NULL);
    }

  struct frame_table_entry *entry = malloc (sizeof (struct frame_table_entry));
  entry->kpage = kpage;
  entry->upage = upage;
  entry->owner = thread_current ();
  entry->pinned = true;

  hash_insert (&frame_table, &entry->elem);

  lock_release (&frame_table_lock);
  return kpage;
}

static void
internal_free_frame_with_lock_held (void *kpage, bool should_free_page)
{
  ASSERT (is_kernel_vaddr (kpage));

  struct frame_table_entry entry_for_find;
  entry_for_find.kpage = kpage;

  struct hash_elem *h_elem = hash_find (&frame_table, &entry_for_find.elem);
  if (h_elem != NULL)
    {
      struct frame_table_entry *entry;
      entry = hash_entry (h_elem, struct frame_table_entry, elem);
      hash_delete (&frame_table, &entry->elem);

      if (should_free_page)
        palloc_free_page (kpage);
      free (entry);
    }
}

static void
internal_free_frame (void *kpage, bool should_free_page)
{
  lock_acquire (&frame_table_lock);
  internal_free_frame_with_lock_held (kpage, should_free_page);
  lock_release (&frame_table_lock);
}

void
free_frame (void *kpage)
{
  internal_free_frame (kpage, true);
}

void
free_frame_without_free_page (void *kpage)
{
  internal_free_frame (kpage, false);
}

void
unpin_frame (void *kpage)
{
  ASSERT (is_kernel_vaddr (kpage));

  lock_acquire (&frame_table_lock);

  struct frame_table_entry entry_for_find;
  entry_for_find.kpage = kpage;

  struct hash_elem *h_elem = hash_find (&frame_table, &entry_for_find.elem);
  if (h_elem != NULL)
    {
      struct frame_table_entry *entry;
      entry = hash_entry (h_elem, struct frame_table_entry, elem);
      entry->pinned = false;
    }
  else
    PANIC ("no frame for the provided kpage");

  lock_release (&frame_table_lock);
}

void
pin_frame (void *kpage)
{
  ASSERT (is_kernel_vaddr (kpage));

  lock_acquire (&frame_table_lock);

  struct frame_table_entry entry_for_find;
  entry_for_find.kpage = kpage;

  struct hash_elem *h_elem = hash_find (&frame_table, &entry_for_find.elem);
  if (h_elem != NULL)
    {
      struct frame_table_entry *entry;
      entry = hash_entry (h_elem, struct frame_table_entry, elem);
      entry->pinned = true;
    }
  else
    PANIC ("no frame for the provided kpage");

  lock_release (&frame_table_lock);
}
