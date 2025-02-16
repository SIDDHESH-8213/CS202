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


int ref_count[PHYSTOP/PGSIZE];  // Reference count array
struct spinlock ref_lock;
extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&ref_lock, "ref_lock"); 
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    //initially set counter to 1
    acquire(&ref_lock);
    ref_count[((uint64)p)/PGSIZE]=1; 
    release(&ref_lock);

    kfree(p);
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa) {
  if (!pa)
      panic("kfree: NULL pointer");

  if (((uint64)pa % PGSIZE) != 0 || (uint64)pa < KERNBASE || (uint64)pa >= PHYSTOP)
      panic("kfree: invalid addr");

  int idx = FRINDEX(pa);
  acquire(&kmem.lock);
  
  if (ref_count[idx] > 1) {
      ref_count[idx]--;
      release(&kmem.lock);
      return;
  }

  ref_count[idx] = 0;
  struct run *r = (struct run*)pa;
  memset(pa, 1, PGSIZE);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}


// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void* kalloc(void) {
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);
  if (r) {
      memset((char*)r, 5, PGSIZE);  // Fill with junk for debugging

      // over here we initialize reference count for a new page
      acquire(&ref_lock);
      ref_count[((uint64)r)/PGSIZE]=1; 
      release(&ref_lock);
  }
  return (void*)r;
}

