# MIT-6.1810-Fall-2022

# Lab: Mmap

这个 lab 有点超出我暂时的能力圈了，完全参考了大佬的实现，以后变更强了再回来重新做。

目标

`mmap` 和 `munmap` 系统调用允许 UNIX 程序管理它的虚拟地址空间，可以被用来在进程间共享内存，将文件映射到程序的地址空间，也可以作为垃圾回收机制的基础。

实现 `mmap` 和 `munmap` 系统调用，本 lab 只关注于增加将文件映射到进程的地址空间的功能。

`mmap` 函数的原型为：

```c
void *mmap(void *addr, size_t length, int prot, int flags,
           int fd, off_t offset);
```

- 第一个参数 `addr` 可以假设永远为 `0`，即由操作系统来决定分配到哪一个虚拟地址
- `mmap` 返回最终将文件映射到的虚拟地址 `addr` 值 或者 返回 `0xffffffffffffffff` 代表映射失败
- 第二个参数 `length` 代表要映射的字节数，该值未必等于文件的长度
- 第三个参数 `prot` 代表映射的内存区域的执行权行：`PROT_READ` 或者 `PROT_WRITE` 或者 两者都有
- 第四个参数 `flags` 的值要么是 `MAP_SHARED` 代表对分配后内存的修改要写回文件；或者 `MAP_PRIVATE` 代表不需要写回。不需要在 `flags` 中额外增加位标志。
- 第五个参数 `fd` 是要映射的文件对应的文件描述符
- 第六个参数 `offset` 代表要映射的偏移量，可以认为它的值就是 `0`

`munmap` 函数的原型为：

```c
void munmap(void *addr, size_t length);
```

`munmap` 应该移除对于指定虚拟地址 `addr` 所属page的映射，如果该进程修改了里面的映射值且 `flags` 的权限被置为 `MAP_SHARED` ，则将该 page 里的数据写回文件。

`munmap` 可以只能覆盖 `mmap` 映射的部分区域，但你可以认为要么从头开始 `munmap` ，要么从尾开始 `munmap`，或者整个区域，不会在中间挖一个洞。

建议

1. 对同一文件且权限为 `MAP_SHARED` 的两个进程的映射，可以不共享一个物理page
2. 给 `mmap` 和 `munmap` 增加足够的功能以通过 `mmaptest` 测试
3. 先 在 UPROGS 中增加 `_mmaptest` ，和 `mmap` 和 `munmap` 系统调用，最初可以先返回 `-1`，在 kernel/fcntl.h 文件中已经定义了 `PROT_READ` 等权限。此时执行 `mmaptest` 会在第一次 `mmap` 的时候返回。
4. `mmap` 采取 lazy allocation 的策略，即最初不会读取文件或者分配内存，而是只更新 page table，以增加 `mmap` 的效率，只有等到真的会读取的时候，再在 `usertrap` 里处理它。
5. 维护为每一个进程映射的区域，定义一个结构体 VMA 对应 Lecture 15 中描述的虚拟内存区域，里面的成员包括有：虚拟地址，长度，权限，映射的文件等等。因为 xv6 内核没有内存分配器，所以可以分配一个固定大小的数组来保存结构体，长度定义成 16 足够使用。
6. 实现 `mmap` 函数：
    1. 在进程的虚拟地址空间中找到一个未使用的区域
    2. 在该进程的映射数组中增加一个 VMA
    3. 该 VMA 中应该包含一个指向被映射文件的指针
    4. 增加该文件的引用计数以便其它进程关闭时该文件不会丢失
    5. 运行 mmaptest ，第一次 mmap 应该成功，但是第一次访问被映射的内存会导致 page fault 然后被 kill 掉。
7. 增加代码来处理访问映射后内存的 page fault 情况：
    1. 分配一个物理内存page
    2. 将 4096 字节相关文件读入该page
    3. 将它映射到用户地址空间
    4. 用 `readi` 函数读取文件，它接收一个 offset 参数代表从文件的哪里开始读，读的时候要对传递给 `readi` 的 inode 获取锁和释放锁
    5. 不要忘记正确设置 page 的权限
    6. 运行 `mmaptest` 指令，它应该能运行到第一个 `munmap` 系统调用处
8. 实现 `munmap` 系统调用：
    1. 找到地址范围对应的 VMA，使用 `uvmunmap` 函数来取消指定的page的映射
    2. 如果 `munmap` 移除了之前 mmap 映射的所有页面，它应该递减对应 struct file 的引用计数。如果映射的页面被修改且权限为 `MAP_SHARED` ，将修改页面写回文件。可以参考 filewrite 作为参考。
    3. 理想状态下，应该只写回权限为 `MAP_SHARED` 且修改了的页面，RISC-V PTE 中的 dirty bit（D）代表当前的页面是否被修改。然而，因为 `mmaptest` 不会检测非修改页面有没有被写回，所以可以直接写回所有页面而无需考虑是否被修改的情况。
9. 修改 `exit` 函数使得它和 munmap 一样取消进程映射的页面，修改后再用 mmaptest ，`mmap_test` 测试应该能通过，但 `fork_test` 还不行。
10. 修改 `fork` 函数保证子进程和父进程有相同的映射区域，记得增加对 VMA's struct file 的引用计数。在子进程的 page fault handler 中，也可以分配一个新的页面而不是跟父进程共享，这样的实现更 cool，但是需要多做一些工作。
11. 此时再运行 `mmaptest` 应该能通过测试。
12. 最后执行 `usertests -q` 确保通过所有测试。

思路

第一步，增加测试程序和系统调用：

1. 在 Makefile 文件中的 UPROGS 注册 `$U/_mmaptest\` 程序
2. 增加 `mmap` 和 `munmap` 系统调用，依次在 `usys.pl` 文件中注册入口，在 `user.h` 文件中声明系统调用，在 `syscall.h` 文件中为系统调用注册号，在 `syscall.c` 文件中增加系统调用。
3. 在 `vm.c` 文件中实现 `mmap` 函数和 `munmap` 函数，然后在 `sysfile.c` 中注册对应的系统调用函数 `sys_mmap` 和 `sys_munmap` ，具体的功能通过调用前面的两个函数实现，为了能够在 `sysfile.c` 中调用两个函数，要在 `def.h` 文件中把它们声明。

第二步，在进程 proc 的结构体中增加 VMA 数组，VMA数组中定义：

- 虚拟地址
- 长度
- 写回与否
- 权限
- 文件描述符

第二步，实现 `mmap` 函数：

1. 在进程的虚拟地址空间中找到一个未使用的区域。调用 walkaddr 函数，传入的参数 va 从第三个虚拟页面page开始，因为第一个是 trampoline，第二个是 trapframe ，如果传入的 va 没有映射到一个物理地址，则 walkaddr 函数会返回 0，根据这一特性就可以找到一个有效的 va 且它对应的 page 没有分配。
2. 在该进程的映射数组中增加一个 VMA。将当前 mmap 要映射的信息维护到进程的映射数组中，维护的信息中要包含一个指向文件的指针。
3. 增加文件的引用计数。

第三步，实现 `munmap` 函数：

第四步，修改 `exit` 函数：

第五步，修改 `fork` 函数：
