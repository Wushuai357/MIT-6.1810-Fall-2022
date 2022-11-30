# MIT-6.1810-Fall-2022

# Lab: Traps

### RISC-V assembly
a2
26:	45b1                	li	a1,12
24:	4635                	li	a2,13
630
38
He110 World%
0x00726c64
no
x=3 y=39660672%
Becauese y is not initialized, so print function just print the value in the memory address of y, which is meaningless.

### Backtrace
目标

实现 `backtrace` 函数，打印当前的系统调用栈返回地址。根据操作系统的知识，每一个进程有一个专属于它的 sp stack pointer 栈指针指向当前的进程栈栈顶，进程内的函数调用，通过将 sp 移动一个栈帧（每一个函数的返回地址、变量等组成一个栈帧）的距离，将前一个函数的返回地址、参数等压入进程栈中，然后跳入下一个函数。进程的栈从高地址往低地址增长，所以 -sp 向栈内压入，+sp 从栈内弹出。

根据讲稿中的内容，xv6中进程栈的结构如下图所示：
Stack
                   .
                   .
      +->          .
      |   +-----------------+   |
      |   | return address  |   |
      |   |   previous fp ------+
      |   | saved registers |
      |   | local variables |
      |   |       ...       | <-+
      |   +-----------------+   |
      |   | return address  |   |
      +------ previous fp   |   |
          | saved registers |   |
          | local variables |   |
      +-> |       ...       |   |
      |   +-----------------+   |
      |   | return address  |   |
      |   |   previous fp ------+
      |   | saved registers |
      |   | local variables |
      |   |       ...       | <-+
      |   +-----------------+   |
      |   | return address  |   |
      +------ previous fp   |   |
          | saved registers |   |
          | local variables |   |
  $fp --> |       ...       |   |
          +-----------------+   |
          | return address  |   |
          |   previous fp ------+
          | saved registers |
  $sp --> | local variables |
          +-----------------+
`sp` 指针指向栈顶。

又由题目中介绍得知，调用 `r_fp` 可以得到当前函数的 栈帧地址 `fp`，且在每一个栈帧内：fp - 8byts 指向该函数的返回地址，fp - 16bytes 指向前一个调用该函数的函数的栈帧地址。

所以，先取到当前的栈帧地址，然后利用遍历的方式一路向上，可以打印出所有栈帧的地址。题目中还额外提供了判断结束的方法：因为每一个进程的栈被存放在一个 page 中，所以得到初始栈帧地址后，可以根据该地址算出该 page 的上限 UP 和下限 DOWN，只有一个栈帧 fp 处在二者之间的时候才打印它。

### Alarm
目标

给 xv6 增加一个周期提醒进程使用 CPU 时间的特性，增加一个系统调用 `sigalarm(interval, handler)` ，当某一进程调用该系统调用的时候 `sigalarm(n, fn)` ，每当该进程执行 `n` 个 ticks 之后，内核应当调用函数 `fn` 。当 `fn` 返回后，该进程继续恢复执行。

当某一进程调用 `sigalarm(0, 0)` 时，内核应当暂停该周期提醒。

分析

alarmtest 执行的流程为：

1. 用户进程 alarmtest 在**用户态**执行，调用系统调用 `sigalarm()`
2. 经由 trampoline 进入**内核态** usertrap，判断 trap 类型是系统调用，跳转到 `syscall()` 函数，根据 `a7` 寄存器值决定调用 `sys_sigalarm()` 函数，跳转到那
3. `sys_sigalarm` 函数取出用户进程调用系统调用时传入的参数，把参数写入进程的结构体中，执行完毕返回到 `syscall` ，`syscall` 执行完毕，继续返回到 usertrap
4. usertrap 额外判断导致进入 trap 的原因是不是因为定时器中断，如果不是，直接返回到 trampoline 中的 sret ，sret 中把第一次进入 trampoline 时写入到进程 p→trapframe 中的用户进程寄存器值写回寄存器
5. 控制权交给用户进程，进入**用户态**，用户进程开始正常执行
6. 定时器中断，进入**内核态**，继续进入 trap，在 trampoline 中保存用户进程状态后跳到 usertrap，进入 trap 的原因是定时器中断，直接在处理定时器中断的函数中增加当前进程的计数器，如果计数器的值等于第一次用户调用 `sys_sigalarm` 时传入的参数值，则调用用户指定的 `periodic` 函数
7. 调用用户指定函数的方式为：把指向该函数的指针（其实就是它的地址）写入 当前进程p 的 trapframe 中的 epc 变量中，它暂存从内核返回时程序计数器（就是调用系统调用 的函数在系统调用返回时下一步要执行的位置）
8. 除了修改 p 的 trapframe 中的 epc 值外，将当前进程的计数器归零，另外，用一个变量做进入拒止，交出 CPU 一段时间后，从 trap 返回，返回到 trampoline 中的 sret ，sret 将 p 的 trapframe 值写回 32 个寄存器中。因此 epc 值被写成了 `periodic` 函数的地址，程序计数器此时就指向 `periodic` 的地址，然后下一步开始执行  `periodic` 函数
9. 进入**用户态**，`periodic` 函数执行完之后调用系统调用 `sys_sigreturn` ，进入 trampoline 保存 32 个寄存器的值，跳到 trap 中，trap 判断这是一个系统调用，然后跳到 sys_sigreturn ，sys_sigreturn 是最后返回前执行的系统调用，它恢复初始进程的 trapframe 值，以便后续 trap 返回的时候恢复到用户进程的初始状态，执行完毕后最后回到 usertrap，usertrap 跳到 usertrapret，再跳到 trampoline 恢复寄存器的值，最后调用 sret 返回到用户进程。

思考

1. 把 `user/alarmtest.c` 注册到 `Makefile` 中，在正确声明 `sigalarm` 和 `sigreturn` 系统调用前，`alarmtest` 不会编译成功。
2. 注册系统调用 `sigalarm` 和 `sigreturn`
3. 在 kernel/sysproc.c 中实现 sigalarm 和 sigreturn 函数框架。初始先不管 sigreturn 函数，先来处理 sigalarm 函数。利用系统提供的两个方法取到用户进程传入的参数，然后直接返回。
4. 真正处理 中断 和实现 alarm 的功能是在 usertrap 函数里，具体的原因可以见前面的分析以及我读 xv6 文档的笔记。
5. 前面的代码对进程状态的 proc 结构体又增加了不少变量.
6. 最后要处理的是 sys_sigreturn 函数，它的作用是在用户调用的 periodic 函数准备返回的时候，把最开始保存的用户进程状态恢复到代表用户进程状态的结构体 trapframe 中，此外，它还恢复代表置位的 alarm_lock，可以让后面的函数进入 periodic。
7. 7. 最后的最后，对于 **test3**，总是报错说 `a0` 寄存器的值被修改，分析题目中给的提示，它说因为 `sys_sigreturn` 函数在返回的时候会把它的返回值写入寄存器 `a0` 中，导致最后恢复的时候 `a0` 的值不对（其实这里的原因我还没有搞清楚，按理来说，sys_sigreturn 返回的时候写入了寄存器 `a0`，根本不影响 trapframe 中的 `a0`，等到整体返回到 trampoline 的时候，会把 trapframe 中的 `a0` 写回寄存器 `a0`，感觉完全不会影响，但是实际就是在这里无限报错！）。最后，我使了比较流氓的一招，既然 `sys_sigreturn` 会把它的返回值写入寄存器 `a0` 导致出错，那我就直接让它返回正确的 `a0` 值，这样，就算是写入覆盖了，也是用正确的覆盖了正确的。
8. 最后，通过了所有的测试。希望以后有机会可以对 `a0` 的问题做一个更合理更优雅地解决，以及明白为什么这里会 `a0` 会出错。
