// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct ref{
  struct spinlock ref_lock;
  uint ref_cnt;
};
struct ref ref[PHYSTOP/PGSIZE];
void
kinit()
{
  for (int i = 0; i < PHYSTOP / PGSIZE; ++i)
  {
	  initlock(&ref[i].ref_lock, "kmem_ref");
	  ref[i].ref_cnt = 1;
  }
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  uint64 pg_num = (uint64)pa / PGSIZE;
  acquire(&(ref[pg_num].ref_lock));
  ref[pg_num].ref_cnt--;
  if (ref[pg_num].ref_cnt > 0)
	{
	  release(&(ref[pg_num].ref_lock));
	  return;
	}
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
  {
  	uint64 pg_num = (uint64)r / PGSIZE;
     	acquire(&(ref[pg_num].ref_lock));
        ref[pg_num].ref_cnt = 1;
	release(&(ref[pg_num].ref_lock));
   	kmem.freelist = r->next;
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

void add_ref_cnt(uint64 pa){
	acquire(&(ref[pa/PGSIZE].ref_lock));
	ref[(uint64)pa/PGSIZE].cnt = ref[(uint64)pa/PGSIZE].ref_cnt + 1;
	release(&(ref[pa/PGSIZE].ref_lock));
}
