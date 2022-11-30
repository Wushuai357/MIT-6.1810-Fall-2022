# MIT-6.1810-Fall-2022

# Lab: Page tables

### usyscall

目标：
1. **映射page**，在负责创建进程虚拟地址空间的函数 `proc_pagetables` 中映射题目要求的 `USYSCALL` page，将这个 page 的权限设置为只读，使用 `mappages` 函数实现该目标。
2. **分配空间并初始化**，在 `allocproc` 函数中为映射的页面分配空间并初始化，初始化的时候根据题目的要求在该页面开头初始化一个结构体 `struct usyscall` （定义在 `memlayout.h` 中），结构体中存放当前进程的 `PID` 。
3. **释放**，在 `freeproc` 函数中释放该页面。

> Which other xv6 system call(s) could be made faster using this shared page? Explain how.

暂时没发现，猜测应该是只访问的信息。

### vmprint
目标
定义函数：void vmprint(pagetable_t); 
按照要求的格式遍历打印进程号为 `1` 的进程的 page table 中的内容。

1. 第一行打印 `vmprint` 函数的参数：`page table 0x0000000087f6b000`
2. page table 是多级页表格式，每一级间多打印一个 .. ，开头打印索引号，然后打印VA和PA，只打印有效的 PTEs

exec
`exec` 初始化一个新的进程，成功则返回 `argc`，先在 `exec.c` 函数的 `return argc;` 之前调用打印函数.

vmprint
为了便于打印开头的地址和每一项层级对应的前缀 .. ，递归函数里加一个参数代表当前递归的深度，xv6 中 page table 最多只有三层，所以如果当前深度 > 2 直接返回即可。
遍历的方式参考了 `freewalk` 函数，此外，打印时的 `%p` 可以打印 64位的地址。

> Explain the output of vmprint in terms of Fig 3-4 from the text. What does page 0 contain? What is in page 2? When running in user mode, could the process read/write the memory mapped by page 1? What does the third to last page contain?
> 

从 page0 ～ page2 的分别是：TRAMPOLINE、TRAPFRAME、USYSCALL。

用户进程不能读写 page1 TRAPFRAME （`kernel/proc.c` 在`pagetable` 映射的时候可以看到权限）。下面是 xv6 的用户进程分布（`kernel/memlayout.h`）.

### pgaccess
目标

实现一个系统调用 pgaccess ，检查哪些用户的 page 被访问过，接收三个参数：指定的起始用户page的虚拟地址、要检测的页面数、一个用户地址用来存检查结果（结果以 bitmap 的形式存放，第一个page对应最低有效位，以此类推）。

1. 在系统调用 pgaccess 中先取到用户传入的三个参数：起始page地址、遍历的页面数和存放遍历结果的地址。
2. 在 riscv.h 中定义 PTE_A ，具体它对应的左移位数查看 risc-v 手册后得知：1 << 6
3. 在将 PA 映射到 VA 的函数 mappages （kernel/vm.c）中把映射后得到的 PTE 置为 PTE_A，代表访问过，因为每一个页面第一次映射就是一次访问，将 PTE_A 置为有效。
4. 在系统调用 pgaccess 中，从用户指定的起始地址开始，使用函数 walk 取到该地址对应的 PTE，然后判断它是否有效且访问过，如果是则将结果对应的该位置为 1（如果访问过，计数后要把访问的位清除），最后把结果使用 copyout 函数写回用户指定的存放结果的地址。
