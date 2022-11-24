// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

#define STEALPAGE 16

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock[NCPU];
  // struct run *freelist;
  struct run *freelist[NCPU];
} kmem;

void
kinit()
{
  static char lock_name[NCPU][7];
  for (int i = 0; i < 8; ++i) {
    snprintf(lock_name[i], 7, "kmem_lock%d", i);
    initlock(&kmem.lock[i], lock_name[i]);
  }
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

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  int cpu_id;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  push_off();
  cpu_id = cpuid();
  pop_off();
  acquire(&kmem.lock[cpu_id]);
  r->next = kmem.freelist[cpu_id];
  kmem.freelist[cpu_id] = r;
  release(&kmem.lock[cpu_id]);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  int cpu_id;
  push_off();
  cpu_id = cpuid();

  acquire(&kmem.lock[cpu_id]);
  r = kmem.freelist[cpu_id];
  if(r) { // 有空闲页
    kmem.freelist[cpu_id] = r->next;
    release(&kmem.lock[cpu_id]);
  } else { // 没有空闲页 从别的CPU空闲页偷 16 页
      release(&kmem.lock[cpu_id]);
      // 一次从 一个或 多个 CPU 偷 STEALPAGE 个页面，
      int steal_page = STEALPAGE;
      struct run *steal_list = 0; // 偷到的链表
      for(int i = 0;i < NCPU; ++ i) {
        if(i != cpu_id) {
          acquire(&kmem.lock[i]);
          // 尝试从CPU[i] 的 freelist 中偷取page
          for(struct run *r = kmem.freelist[i];r && steal_page; r = kmem.freelist[i]) {
            kmem.freelist[i] = r->next;
            r->next = steal_list;
            steal_list = r;
            --steal_page;
          }
          release(&kmem.lock[i]);
          if(steal_page == 0)
            break;
        }
      }
      if(steal_list != 0) {
        r = steal_list;
        // 置换链表
        acquire(&kmem.lock[cpu_id]);
        kmem.freelist[cpu_id] = r->next;
        release(&kmem.lock[cpu_id]);
      }
  }
  
  pop_off();
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
