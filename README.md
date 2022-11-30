# MIT-6.1810-Fall-2022

# Lab: Multithreading

### Uthread: switching between threads
目标

设计一个用户层次的线程 context swtich 机制并实现它。

1. 主要的相关代码在 `user/uthread.c` 和 `user/uthread_switch.S` 中，`uthread.c` 中包含大部分的用户层次线程包以及三个小测试。
2. 需要完善 `user/uthread.c` 中的函数 `thread_create()` 和 `thread_schedule()`，以及在 `user/uthread_switch.S` 中的函数 `thread_switch()` 。
3. 第一个目标是当 `thread_schedule()` 第一次执行一个线程的时候，它在自己的栈中执行传给它的函数 `thread_create()` 。
4. 第二个目标是保证 `thread_switch()` 保存将要被切出去的线程的寄存器值，恢复要被换进来的线程的寄存器值，然后返回到换进来线程上一次被切出去的位置。
5. 需要决定在哪里保存和恢复寄存器值，修改 `struct thread` 来保存寄存器值是个不错的主意。
6. 需要在 `thread_schedule()` 添加对 `thread_switch()` 的调用，可以给`thread_switch()` 传你需要的参数，目的就是从一个线程切换到下一个线程。
7. 使用 uthread 指令测试实现结果。

提示

1. `thread_switch` 只需要保存和恢复那些 callee-save 的寄存器值。
    
    > **为什么？**
    > 
    > 
    > Because the Caller register is saved in thread_schedule's stack by the c compiler.
    > 
2. 可以看 `user/uthread.asm` 中的 `uthread` 汇编代码，作为 **debug** 的参考。

### Using threads

目标

在真实的 Linux 或者 Mac 系统中完成这个lab，使用 UNIX 中的 pthread 线程库，可以在命令行中使用 `man pthread` 指令找到文档。

在文件 `notxv6/ph.c` 包含有一个单线程的哈希表，多线程使用时会出错，在主目录中 `make ph` 编译该文件，然后使用 `./ph 1` 测试它。后面的参数代表同时操作它的线程数，如果正确会有下面类似的输出：

```
100000 puts, 1.574 seconds, 63531 puts/second
0: 0 keys missing
100000 gets, 1.524 seconds, 65619 gets/second
```

思路

1. 定义一个全局锁。对于单个 bucket 甚至单个 entry 使用一把锁可以进行加速，但是这里实现有点问题，时间又有限，暂时先做到这个程度，以后可以继续优化。
2. 在 main 函数中初始化该锁。
3. 在 put 函数中，用锁包住操作表项的语句。因此 get 操作并不修改表项，所以不需要用锁来保护表项。

### Barrier
目标

实现一个 barrier，提前到达的线程必须等待某一条件成立。

思路

题目中的代码已经初始化了锁和 condition variable，只要操作判断条件即可。

如果当前线程不满足条件的时候就让它进入 sleep，直到所有线程都 sleep 的时候 ++round 代表一轮。
