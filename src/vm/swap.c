#include <bitmap.h>
#include "vm/swap.h"
#include "devices/block.h"
#include "threads/vaddr.h"
#include "threads/synch.h"

static struct block *swap_device;
static struct bitmap *swap_table;
static struct lock swap_lock;

static const size_t block_sectors_per_page = PGSIZE / BLOCK_SECTOR_SIZE;

void
swap_init (void)
{
  lock_init (&swap_lock);
  swap_device = block_get_role (BLOCK_SWAP);
  swap_table = bitmap_create (block_size (swap_device) / block_sectors_per_page);
  bitmap_set_all (swap_table, false);
}

void
swap_destroy (void)
{
  bitmap_destroy (swap_table);
}

void
swap_in (size_t index, void *addr)
{
  ASSERT (is_kernel_vaddr (addr));
  lock_acquire (&swap_lock);

  if (bitmap_test (swap_table, index) == false)
    PANIC ("empty slot");
  size_t i;
  for (i = 0; i < block_sectors_per_page; i++)
    {
      block_read (swap_device, index * block_sectors_per_page + i, addr + BLOCK_SECTOR_SIZE * i);
    }
  bitmap_set (swap_table, index, false);
  lock_release (&swap_lock);
}

size_t
swap_out (void *addr)
{
  ASSERT (is_kernel_vaddr (addr));
  lock_acquire (&swap_lock);

  size_t index = bitmap_scan (swap_table, 0, 1, false);
  size_t i;
  for (i = 0; i < block_sectors_per_page; i++)
    {
      block_write (swap_device, index * block_sectors_per_page + i, addr + BLOCK_SECTOR_SIZE * i);
    }
  bitmap_set (swap_table, index, true);
  lock_release (&swap_lock);
  return index;
}

void
swap_free (size_t index)
{
  lock_acquire (&swap_lock);
  if (bitmap_test (swap_table, index) == false)
    PANIC ("empty slot");
  bitmap_set (swap_table, index, false);
  lock_release (&swap_lock);
}
