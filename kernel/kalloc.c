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

struct run
{
  struct run *next;
};

struct
{
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct
{
  struct spinlock lock;
  uint8 counter[PA2COWINDEX(PHYSTOP) + 1];
} pa_ref_count;

void increase_count(uint64 pa)
{
  acquire(&pa_ref_count.lock);
  ++pa_ref_count.counter[PA2COWINDEX(pa)];
  release(&pa_ref_count.lock);
}

uint64 decrease_count(uint64 pa) {
  int ret;
  acquire(&pa_ref_count.lock);
  ret = --pa_ref_count.counter[PA2COWINDEX(pa)];
  release(&pa_ref_count.lock);
  return ret;
}

void kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&pa_ref_count.lock, "cow");
  freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  // copy-on-write start:
  // 初始化物理页面计数器数组的值 且初始化为 1 随后调用 kfree 的时候直接释放
  for (int i = 0; i < ((sizeof(pa_ref_count.counter)) / sizeof(uint8)); ++i) {
    pa_ref_count.counter[i] = 1;
  }
  // copy-on-write end.
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa)
{ 
  int res;
  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // copy-on-write start:
  res = decrease_count((uint64)pa);
  if (res < 0)
    panic("kfree: decrease counter");
  if (res > 0)
    return;
  // copy-on-write end.

  // 只有当 decrease 之后计数值 == 0 的时候 才真的释放物理页面
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;

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
  if (r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  // copy-on-write start:
  if (r) {
    memset((char *)r, 5, PGSIZE); // fill with junk
    increase_count((uint64)r); // 增加物理页面的计数器
  }
  // copy-on-write end.

  // if (r) 
  //   memset((char *)r, 5, PGSIZE); // fill with junk
  return (void *)r;
}
