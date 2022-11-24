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
// 1. 先用 GetBucket 找到相应的 bucket，然后在这个 bucket 中寻找 相应的 dev he blockno
// 2. 能找到就返回相应的 b， 否则就得 对所有 bucket 进行遍历，采用 LRU 做 eviction
// 在 eviction 中采取，总是只拿一个锁，来避免死锁问题
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int key = getHashKey(dev, blockno);  // GetHashKey(dev, blockno);
  struct BucketType *bucket = bucket = &bcache.buckets[key];
  // Is the block already cached?
  acquire(&bucket->lock);
  for (b = bucket->head.next; b; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bucket->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  // 我们得先释放bucket锁，不然在占用一个锁的情况下，再 acquire bucket.lock 会有死锁问题
  release(&bucket->lock);

  // 下面是正式的 eviction，我们扫描所有的 bucket 然后找到最大的 timestamp，之后进行替换
  struct buf *before_latest = 0;  // res 是我们要替换到这个 bucket 的节点
  uint timestamp = 0; // timestamp 是最大的 b->timestamp, 也就是 LRU 算法了
  struct BucketType *max_bucket = 0;  // b 所在的 bucket

  for(int i = 0;i < BUCKETSIZE; ++ i) {
    int find_better = 0;
    struct BucketType *bucket = &bcache.buckets[i];
    acquire(&bucket->lock);
    // 遍历 per bucket
    for (b = &bucket->head; b->next; b = b->next) {
      if (b->next->refcnt == 0 && b->next->timestamp >= timestamp) {
          before_latest = b;
          timestamp = b->next->timestamp;
          find_better = 1;
      }
    }
    // 如果这个 bucket 中有更大 的 timestamp，那么我们就采用
    // 注意，我们不能每一次都 release，对于持有最大的 timestamp 的 bucket，我们得一直保持lock
    // 直到被替代，或者结束遍历
    if (find_better) {
      if (max_bucket != 0) release(&max_bucket->lock);
      max_bucket = bucket;
    } else
      release(&bucket->lock);
  }
  // 如果我们找到了 一个 buf，我们就先从这个桶删除 这个 buf
  // 并释放锁
  struct buf * res = before_latest->next;
  if (res != 0) {
    before_latest->next = before_latest->next->next;
    release(&max_bucket->lock);
  }
  // 现在我们将 拿到的 buf，插入到 需要 dev 的 bucket 中
  acquire(&bucket->lock);
  if (res != 0) {
    res->next = bucket->head.next;
    bucket->head.next = res;
  }
  // 如果有另一个线程先进入了 eviction，并且也正好是这个 dev 和 blockno，
  // 我们再检查一遍，确保不会让 一个 dev 对应一个 buf
  for (b = bucket->head.next; b ; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bucket->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  if (res == 0) panic("bget: no buffers");
  // 如果上面保证了还是找不到，那么就说明不会有重复，我们直接返回这个 buf（res)
  res->valid = 0;
  res->refcnt = 1;
  res->dev = dev;
  res->blockno = blockno;
  release(&bucket->lock);
  acquiresleep(&res->lock);
  return res;
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


