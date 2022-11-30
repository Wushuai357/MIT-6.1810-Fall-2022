# MIT-6.1810-Fall-2022

# Lab: COW

### 目标

写一个实现 copy-on-write 的 fork() 函数，当调用该函数的时候，给子进程创建一个 page table，里面的PTEs其实都指向父进程数据页的物理地址，且将父子进程的 PTEs 都标为`只读`。

假如其中一个进程想要写入的时候，产生一个 page fault 的异常，kernel 中处理 page fault 的 handler 检测出异常来自于 COW 的写入操作，为产生 page fault 的页面在内存中分配一个新的物理 page，将初始 page 的数据拷贝到新分配的 page，然后让产生 page fault 异常的 PTE 指向这个新的物理 page，将权限设置为 `可写` 。返回进程，重新执行写入操作就正常。

COW 使得释放物理内存变得复杂，xv6 相对简单因为它会在没有虚拟地址指向某一物理page的时候释放该page，实际情况中要解决这个问题十分复杂。

### 建议

1. 修改 `uvmcopy` 函数，将父进程的物理页面映射给子进程，清空父进程所有 page 的 PTE_W 权限。使用 RISC-V PTE 中的 RSW (reserved for software) 位 来代表当前 PTE 是一个 copy-on-write 的PTE。
2. 修改 `usertrap` 使得能够识别 page fault 的情况。当一个 write-page-fault发生的时候，用 `kalloc` 分配一个新page，将旧page复制到新page里，将新page插入到引发 write-page-fault 的进程的page table里，权限设置为 PTE_W 。初始权限为 `只读` 的页面，应该保持 `只读` ，任何想要写入该页面的进程都被kill掉。
3. 保证当一个物理page的最后一个 PTE 被覆盖的时候，该物理page应当被 free，但在此之前不能删除。一个较好的实现方式是在 `kalloc` 的时候为每一个物理page 维护一个 reference count，记录指向它的 user page table 数量，假如 == 0， 就释放该页面。当 fork 导致子进程共享父进程的该物理page的时候，增加计数器；当任意进程从page table里移除该映射的时候，递减计数器。 kfree 应当只在该物理page的 reference count == 0 的时候将它放到 free list 中。也可以把这个 count 放到一个数组中，需要自己设计一个方法来索引数组以及选择合适的大小，比如：可以用 page 的物理地址除以 4096 当作它的索引，数组的大小等于 `kalloc` 中的 `kinit` 函数中为 free list 分配的最高物理地址除以 4096.
4. 记得修改 `kalloc` 和 `kfree` 函数来维护 reference count 。
5. 修改 copyout 使得当它遇到 COW page 的时候使用相同的策略。
6. 在 `kernel/riscv.h` 的后面有一些有用的 macros 和 page table 标志的定义。
7. 如果发生 COW 且当前没有空闲内存的时候，引发 COW 的进程应当被 kill。
8. 使用 cowtest 和 usertests -q 测试你的实现。

### 思路

1. 设计基础变量。在 `riscv.h` 文件中添加 COW 位和用于获取指定物理地址计数数组索引的宏。
2. 修改 `uvmcopy`。如果父进程的权限为可写，先擦除父进程的权限，将父进程的 RSW(8) 置为 1，代表这是 copy-on-write 的只读，当发生 COW page fault 的时候，可以根据这一位的情况判断原有 page 本身就是只读（写入只读page，杀掉该进程），还是 COW 只读。获取父进程的权限及物理page地址，将父进程的物理page映射到子进程的 page table 中，权限和父进程相同。增加当前分配的物理页面的引用计数器（为后续释放该页面做准备）。
3. 修改 `usertrap`

根据 scause 寄存器的值（`r_scause()` 获得）判断是否发生 page fault ，根据我在谷歌搜出来的资料：

Encoded in SCAUSE, or r_scause() in xv6
• 12: page fault caused by an instruction fetch
• 13: page fault caused by a read
• 15: page fault cause by a write

用 `r_stval()` 获得引发异常时指令地址；

调用自己写的 handler，传入引发异常进程的 pagetable；

进入 handler，如果访问无效 PTE 或者 非COW page fault的 PTE，直接返回 -1，后面会kill掉；

获取该 va 对应的物理page；

在内存空间中分配一个新的物理内存page；

将旧的值复制一份到新的page中；

获取当前物理page对应的PTE值，擦除它的 COW 位，增加可写权限；

调用 kfree 减少旧物理地址的引用，如果减少后引用 == 0，修改后的 kfree 会自动释放它；

如果一切执行正常，返回 0 代表成功。

4. COW 释放机制

kalloc 开头：

在 kalloc 中实现一个计数数组，数组长度把 PHYSTOP 传入前面定义的宏中获得 ，因为 xv6 中定义的物理内存的范围是 [KERNBASE, PHYSTOP]，KERNBASE 是地址空间中 kernel 区块结尾的下一个地址，PHYSTOP 定义在 `kernel/memlayout.h` 中，代表物理内存的最高地址。每 PGSIZE 个地址（4096 字节）被分成一个 page，总共需要的 page 数就对应计数数组的长度。

此外，数组还需要一个全局锁，只有持有锁的进程才能操作数组中的值。
freerange：

在最开始的时候，需要初始化整个数组中的值。因为，`kinit` 函数在整个系统启动的时候会从头到尾释放一遍内存，调用 `freerange` 函数，`freerange` 在释放单个页面的时候会调用 `kfree` 函数，根据 COW 的释放机制，kfree 会递减一个物理页面的计数器，只有当它 == 0 的时候才会真的释放该页面。第一次调用 freerange 的时候，显然整个物理页面计数器数组都还没初始化，所以，先在 freerange 里初始化整个数组，且将初始值设置为 1 ，这样，它调用 kfree 的时候刚好将其值递减为 0，然后真的释放一遍。（因为这里 kfree 的功能是不可更改的，别的代码也会调用它，所以修改 freerange 来配合它，它要先递减计数器，所以要在它之间就必须要初始化计数器数组。）
kfree：

修改 kfree，释放操作先递减物理页面计数器，只有当计数器值 == 0（返回值 == 0）的时候才真的释放该物理页面。
kalloc：

修改 kalloc，分配物理页面的时候，递增它的计数器。
4. 修改 copyout

copyout 从内核地址空间复制数据到用户地址空间，这里之所以要专门覆盖 copyout 函数，因为它直接进行内存操作，不走系统调用的流程，所以不会进入前面设计的 trap handler 的流程，所以如果不专门处理就会出错。

处理的流程也很简单，先对要写入的用户地址空间地址调用 cow trap handler，其中的各种测试会保证当前写入的要写入的目的地址是有效的，且指向的一个 COW 的页面的时候，就会先处理该异常，然后让 va 指向新的物理页。
