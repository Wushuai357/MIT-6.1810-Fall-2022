# MIT-6.1810-Fall-2022

# Lab: Locks

### 目标

重新设计代码以增加并行性，通过修改数据结构和锁的策略以减少获取锁的次数。

### Memory allocator
思路
1. 重新设计内存分配器和 free-list 的锁，每一个 CPU 维护一个 free-list，每一个 list 有它自己的锁，这样一来，不同 CPU 上的分配器就可以并行工作。这种方案要解决的问题就是如果一个 CPU 上的 free-list 用完了，而别的还有，就需要从别的地方偷一些内存过来，偷内存牵扯到获取该 free-list 锁的问题，但也比最初的全局一把锁要高效一些，因为偷的概率少。
2. 每一个锁的名字都以 kmem 开头，每一个锁都要初始化，使用 `kalloctest` 来测试实现方案有没有减少锁的竞争，使用 `usertests sbrkmuch` 测试是否依然能够将所有的内存分配出去，确定通过 `usertests -q` 中所有测试，最后 `make grade` 应当提示通过。
3. 可以使用 NCPU 代表当前的 CPU 数。
4. freerange 将所有的空闲内存都分配给调用 freerange 的 CPU。
5. cpuid 返回当前 CPU 的编号，但该函数仅在中断关闭的时候可以安全使用，使用 push_off 和 pop_off 来关闭和打开中断。
6. 参考 kernel/sprintf.c 中的 snprintf 获得字符串格式的思路，用来给锁命名，虽然直接用 kmem 也可以。
7. 使用 xv6 的 race detector 来测试你的代码.
8. 或者在你的系统中，你可以将 back trace 转换成对应的代码行数.

### Buffer cache
目标

缓存区的初始结构为：结构体 buf 以双向链表的结构组织，按照 LRU 的策略排序，头节点的下一个节点是最新使用的节点，尾节点是最远未使用的节点。

整个双向链表全局一把锁，如果多个进程同时高强度地读写文件系统，则在缓存区的锁 `bcache.lock` 会面临高频的竞争的状态，从而影响读写缓存的效率。

本实验的目的就是重新设计缓存区的数据结构以增加并发与并行的效率，目标是将所有测试中获取锁但等待的总次数降到 500 以下。

提示

1. 修改 `bget` 函数，它返回传入参数 dev 和 blockno 对应的缓存 buf；如果缓存中没有保存该 物理块，则从缓存中驱逐最久未使用的 且 引用值为 0 的 buf，然后将 dev 和 blockno 对应的缓存维护进去。
2. 修改 `brelse` 函数，只有一个 buf 的引用值 == 0 的时候，才释放它。
3. 难点在于 `bget` 和 `brelse` 的时候，需要合理地获取和释放相关的锁，否则很容易进入死锁的状态。
4. 建议使用一组桶，每一个 buf 根据它的 dev 和 blockno 值算出哈希值，存放到相应的桶中，每一个桶一个锁，这样相较于全局一把锁能提高效率。桶的数量应当是一个素数来减少哈希表碰撞，比如 13 等。
5. 在桶中查找某一个 buf 是否存在，当 buf 不存在为该 dev 和 blockno 值分配一个buf 的操作应该是原子的，即要持有桶的锁。
6. Remove the list of all buffers (bcache.head etc.) and don't implement LRU. With this change brelse doesn't need to acquire the bcache lock. In bget you can select any block that has refcnt == 0 instead of the least-recently used one.
7. bget 中可以采取顺序查找的方式找想要的 buf，如果找不到，释放持有的锁新建一个buf。
8. 有时可能需要持有两把锁，主要不要陷入死锁的状态。
9. debugging tips：先保留全局锁，锁全局，直到判断没有发生竞态条件的时候，再去掉全局锁。

思路

参考提示，计划设置 13 个 bucket，每一个 bucket 一把锁，每一个 buf 根据哈希值算它归属的 bucket，刚开始初始化的时候先把所有的 buf 都初始化到 bucket[0] 。

在 bget 中用 dev 和 blockno 计算对应的 buf 归属的 buckets key，然后先到该 key 对应的 bucket 中遍历，看有没有对应的 buf，如果找到就返回。

如果找不到，说明不存在，需要先驱逐一个 引用 == 0 且最少使用的 buf，最少使用的方式不使用双链表来维护，而是在 buf 中增加一个变量 timestamp，代表一个 buf 上次操作到现在的时间，所以 timestamp 最大值且 引用 == 0 的 buf 就是要驱逐的。

遍历所有的桶找到之后驱逐，然后把要新建的 buf 插入它属于的桶，最后检查一遍返回。

随时要注意获取和释放恰当的锁。

未来可改进的地方

bucket 的大小太小，如果可以类似于 Leetcode 中 LRU 的算法，实现一个哈希表以键 + 节点指针的方式维护一个键表，就可以在知道键的时候 O(1) 地取到该键对应的节点，同时以双向链表的方式维护缓存节点也可以 O(1) 地维护 LRU 的顺序。
