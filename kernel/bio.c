// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define BUCKETSIZE 13

struct BucketType {
  struct spinlock lock;
  struct buf head;
};

struct {
  struct buf buf[NBUF];
  // buckets of all buffers
  // index by fash function of its (dev + blockno) % BUCKETSIZE
  // each bucket has its lock and buffers within buffers 
  // were organized as linked-list
  struct BucketType buckets[BUCKETSIZE];
} bcache;

inline int getHashKey(int dev, int blockno) {
  return (dev + blockno) % BUCKETSIZE;
}

void
binit(void)
{
  static char bcachelockname[BUCKETSIZE][9];
  struct buf *b;

  // create and initialize lock for each bucket
  // name after bcache_ with its bucket number
  for(int i = 0;i < BUCKETSIZE; ++i) {
    snprintf(bcachelockname[i], 9, "bcache_%d", i);
    initlock(&bcache.buckets[i].lock, bcachelockname[i]);
  }

  for(int i = 0;i < NBUF; ++ i)
    bcache.buf[i].timestamp = ticks; // 0

  // create linked list of buckets
  for (int i = 0; i < BUCKETSIZE; ++i) {
    bcache.buckets[i].head.next = 0;
  }

  // 将所有的 buf 都先放到 buckets[0] 中
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.buckets[0].head.next;
    initsleeplock(&b->lock, "buffer");
    bcache.buckets[0].head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
// static struct buf*
// bget(uint dev, uint blockno)
// {
//   struct buf *b;

//   acquire(&bcache.lock);

//   // Is the block already cached?
//   for(b = bcache.head.next; b != &bcache.head; b = b->next){
//     if(b->dev == dev && b->blockno == blockno){
//       b->refcnt++;
//       release(&bcache.lock);
//       acquiresleep(&b->lock);
//       return b;
//     }
//   }

//   // Not cached.
//   // Recycle the least recently used (LRU) unused buffer.
//   for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
//     if(b->refcnt == 0) {
//       b->dev = dev;
//       b->blockno = blockno;
//       b->valid = 0;
//       b->refcnt = 1;
//       release(&bcache.lock);
//       acquiresleep(&b->lock);
//       return b;
//     }
//   }
//   panic("bget: no buffers");
// }


// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  int key;
  struct buf *b;
  struct BucketType *bucket;

  key = getHashKey(dev, blockno);
  bucket = &bcache.buckets[key];
  acquire(&bucket->lock);
  for (b = bucket->head.next; b; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      ++b->refcnt;
      release(&bucket->lock);
      // 保护 b 的缓冲区内容 确保只有一个进程在读写当前的缓冲区 b release 的时候释放该锁
      acquiresleep(&b->lock); 
      return b; 
    }
  }
  // Not cached.
  // 先释放当前桶的锁 再去找最大的 timestamp 对应的桶的锁
  // 如果不释放就去获取另一个锁 可能会导致死锁
  release(&bucket->lock);

  // search through the buckets 
  // Evict the least recently used buf
  // which has the biggest time stamp
  struct buf *before_to_be_evicted = 0; // buf before to be evicted buf because its a single linked-list
  struct buf *to_be_evicetd = 0; // buf to be evicted
  struct BucketType *bucket_to_be_evicted = 0; // bucket contains the to be evicted buf
  uint timestamp = 0; // biggest time stamp
  
  for (int i = 0; i < BUCKETSIZE; ++i) {
    // search through each bucket
    struct BucketType* bkt = &bcache.buckets[i];
    acquire(&bkt->lock);
    int isfound = 0;
    for (b = &bkt->head; b->next; b = b->next) {
      if (b->next->refcnt == 0 && b->next->timestamp >= timestamp) {
        before_to_be_evicted = b;
        to_be_evicetd = b->next;
        timestamp = to_be_evicetd->timestamp;
        isfound = 1;
      }
    }
    if (isfound) {
      if (bucket_to_be_evicted != 0) 
        release(&bucket_to_be_evicted->lock); // 先释放之前持有的锁
      bucket_to_be_evicted = bkt;
    } else {
      release(&bkt->lock);
    }
  }

  // 将要删除的 buf 从当前的链表中删除
  if (to_be_evicetd != 0) {
    before_to_be_evicted->next = to_be_evicetd->next;
    release(&bucket_to_be_evicted->lock);
  }

  // 将 buf 插入属于它的 bucket 中
  acquire(&bucket->lock);
  if (to_be_evicetd != 0) {
    to_be_evicetd->next = bucket->head.next;
    bucket->head.next = to_be_evicetd;
  }
  // 重新检查一遍 确保不会有两个 def 对应一个 buf
  for (b = bucket->head.next; b; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      ++b->refcnt;
      release(&bucket->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  if (to_be_evicetd == 0)
    panic("bget: no buffers!");
  // 如果检查没有异常 直接返回要驱逐的 buf
  to_be_evicetd->valid = 0;
  to_be_evicetd->refcnt = 1;
  to_be_evicetd->dev = dev;
  to_be_evicetd->blockno = blockno;
  release(&bucket->lock);
  acquiresleep(&to_be_evicetd->lock);
  return to_be_evicetd;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  int key;

  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  key = getHashKey(b->dev, b->blockno);
  acquire(&bcache.buckets[key].lock);
  b->refcnt--;
  if (b->refcnt == 0)
    b->timestamp = 0;
  release(&bcache.buckets[key].lock);
}

void
bpin(struct buf *b) {
  int key = getHashKey(b->dev, b->blockno);
  acquire(&bcache.buckets[key].lock);
  b->refcnt++;
  release(&bcache.buckets[key].lock);
}

void
bunpin(struct buf *b) {
  int key = getHashKey(b->dev, b->blockno);
  acquire(&bcache.buckets[key].lock);
  b->refcnt--;
  release(&bcache.buckets[key].lock);
}


