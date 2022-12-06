#include <stdio.h>
#include <hash.h>
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "vm/frame.h"

static struct lock frame_table_lock;
static struct hash frame_table;

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

void *
allocate_frame (enum palloc_flags flags, void *upage)
{
  ASSERT (is_user_vaddr (upage));

  lock_acquire (&frame_table_lock);

  void *kpage = palloc_get_page (flags);
  if (kpage == NULL)
    {
      // TODO: swap 기능 구현 필요
      lock_release (&frame_table_lock);
      return NULL;
    }

  struct frame_table_entry *entry = malloc (sizeof (struct frame_table_entry));
  entry->kpage = kpage;
  entry->upage = upage;
  entry->owner = thread_current ();
  entry->pinned = true; // TODO: 추후에 eviction 해도 문제없을 때 unpin 필요

  hash_insert (&frame_table, &entry->elem);

  lock_release (&frame_table_lock);
  return kpage;
}

static void
internal_free_frame (void *kpage, bool should_free_page)
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
      hash_delete (&frame_table, &entry->elem);

      if (should_free_page)
        palloc_free_page (kpage);
      free (entry);
    }

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
