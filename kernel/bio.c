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
#define BUCK_SIZ 13
#define BCACHE_HASH(dev, blk) (((dev << 27) | blk) % BUCK_SIZ)

struct {
  struct spinlock lock[BUCK_SIZ];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head[BUCK_SIZ];
} bcache;

void
binit(void)
{
  struct buf *b;

  for(int i=0; i<BUCK_SIZ; ++i){
    initlock(&bcache.lock[i], "bcache");
    bcache.head[i].prev = &bcache.head[i];
    bcache.head[i].next = &bcache.head[i];
  }

  // Create linked list of buffers
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head[0].next;
    b->prev = &bcache.head[0];
    initsleeplock(&b->lock, "buffer");
    bcache.head[0].next->prev = b;
    bcache.head[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  uint key = BCACHE_HASH(dev,blockno);

  acquire(&bcache.lock[key]);

  // Is the block already cached?
  for(b = bcache.head[key].next; b != &bcache.head[key]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[key]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  release(&bcache.lock[key]);
  struct buf *res = 0;
  int lru_bkt = -1;
  for(int i=0; i<BUCK_SIZ; ++i){
    acquire(&bcache.lock[i]);
    int found_new = 0;
    for(b = bcache.head[i].prev; b != &bcache.head[i]; b = b->prev){
      if(b->refcnt == 0 && (!res || b->lst_use < res->lst_use)){
        res = b;
        found_new = 1;
        // break;
      }
    }
    if(!found_new){
      release(&bcache.lock[i]);
    }else{
      if(lru_bkt != -1) release(&bcache.lock[lru_bkt]);
      lru_bkt = i;
    }
  }
  if(res){
    res->next->prev = res->prev;
    res->prev->next = res->next;
    release(&bcache.lock[lru_bkt]);
    acquire(&bcache.lock[key]);

    for(b = bcache.head[key].next; b != &bcache.head[key]; b = b->next){
      if(b->dev == dev && b->blockno == blockno){
        b->refcnt++;
        bcache.head[key].prev->next = res;
        res->next = &bcache.head[key];
        res->prev = bcache.head[key].prev;
        bcache.head[key].prev = res;
        release(&bcache.lock[key]);
        acquiresleep(&b->lock);
        return b;
      }
    }
    res->next = bcache.head[key].next;
    bcache.head[key].next->prev = res;
    res->prev = &bcache.head[key];
    bcache.head[key].next = res;
    res->dev = dev;
    res->blockno = blockno;
    res->valid = 0;
    res->refcnt = 1;
    release(&bcache.lock[key]);
    acquiresleep(&res->lock);
    return res;
  }

  panic("bget: no buffers");
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
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  uint key = BCACHE_HASH(b->dev, b->blockno);
  acquire(&bcache.lock[key]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->lst_use = ticks;
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head[key].next;
    b->prev = &bcache.head[key];
    bcache.head[key].next->prev = b;
    bcache.head[key].next = b;
  }
  
  release(&bcache.lock[key]);
}

void
bpin(struct buf *b) {
  uint key = BCACHE_HASH(b->dev, b->blockno);
  acquire(&bcache.lock[key]);
  b->refcnt++;
  release(&bcache.lock[key]);
}

void
bunpin(struct buf *b) {
  uint key = BCACHE_HASH(b->dev, b->blockno);
  acquire(&bcache.lock[key]);
  b->refcnt--;
  release(&bcache.lock[key]);
}


