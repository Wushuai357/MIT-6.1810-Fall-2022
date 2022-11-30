# MIT-6.1810-Fall-2022

# Lab: System Calls

## using GDB

### booting GDB
在一个终端输入：make qemu-gdb ，启动GDB。
在另一个终端输入：gdb-multiarch 或者 riscv64-linux-gnu-gdb。
因为我的电脑是 20年的 M1 MacBookair，最后安装的 RISC-V 工具包是 riscv64-unknown-elf-gcc ，
所以这里我运行 GDB 时的指令是：riscv64-unknown-elf-gdb。
即前缀和要本机安装的工具包名字对应。

### 系统调用栈
指令：`b 函数名/*地址` 可以在指定的位置打断点，注意不要漏掉地址前面的 `*` ；

指令：`layout XXX` 可以新开一个窗口展示当前执行到的代码，`XXX` 猜测对应的是代码的格式，比如 `layout src` 看源码，`layout asm` 看汇编代码。这里指针指向的是下一步要执行的代码；

指令：`backtrace` 展示函数调用栈，栈顶在上，栈底在下；

指令：`c` 代表 continuing，让程序开始执行，直到遇到自己的打的断点；

指令：`n` 代表 next，往下执行一步。

### 系统调用寄存器
指令：`p /x *p` 以十六进制打印当前进程的所有状态（进程信息、寄存器、打开的文件列表等）；

> RISC-V 中通过 `ecall` 指令进行 Syscall 的调用。 `ecall` 指令会将 CPU 从用户态转换到内核态，并跳转到 Syscall 的入口处。通过 a7 寄存器来标识是哪个 Syscall。至于调用 Syscall 要传递的参数则可以依次使用 a0-a5 这 6 个寄存器来存储。
> 

所以写入 `a7` 寄存器的值代表的是要执行的系统调用的编号，在 `kernel/syscall.h` 文件中定义。从该文件中查询得知，汇编代码中写入的 `SYS_exec` 对应的值为 `7` ，含义是创建一个新的进程。

### 进程状态
根据操作系统的知识，进程分为内核进程和用户进程，分别运行在 kernel mode 和 user mode 中，本案例中是 user process 通过系统调用进入了 kernel mode ，所以之前处在 user mode 中。

### panic
注意：做这一题之前，要真的在 `kernel/syscall.c` 文件中把 `num = p->trapframe->a7;` 改成 `num = * (int *) 0;` 才行，然后重新编译一下，才会编译报错：

```
xv6 kernel is booting

hart 2 starting
hart 1 starting
scause 0x000000000000000d
sepc=0x0000000080001ff8 stval=0x0000000000000000
panic: kerneltrap
```

注意：打断点的时候要打在编译报错输出的 sepc 值中，如上面提示段 所示的`sepc=0x0000000080001ff8` 才行，而不是题目中说的 sepc=0x0000000080001ff8 ，这不是一个通用的位置。

kernel 之所以崩溃的原因，是因为 num 本来存的是要调用的系统调用值，这里直接取了地址空间中地址 `0` 处的值，因为虚拟地址空间 `0` 处其实是不可访问的，代表空指针的意思，一旦访问，立马系统异常然后被中断，所以这里 kernel 崩溃了。

同时，上面的 `scause 0x000000000000000d` 值也证实了这个观点，`scause` 代表引发系统异常的类型，`0x000000000000000d` 对应的类型为 Load access fault 。

### process 状态
### 

指令：`p p→name` 打印当前 the name of the binary；

指令：`p p→pid` 打印当前进程的 process id 。

## trace
当用户进程调用系统调用时，先被 `user/usys.pl` 编译后的 `usys.S` 文件阻塞，这个汇编文件里执行的操作为：

1. riscv 指令集下 `li` 指令移动要执行的系统调用 number （定义在 `kernel/syscall.h` 中）到寄存器 a7 中
2. 使用 riscv 指令集中的 `ecall` 激活系统调用
3. 执行完系统调用后 `ret` 返回

系统调用被声明在 `user/user.h` 文件中，所以，为了实现自己的系统调用，需要把该系统调用的声明也加在 `user/user.h` 文件中。

进入内核模式后，会把系统调用导向在 `kernel/syscall.c` 和 `kernel/syscall.h` 中该系统调用函数对应的函数上。为了实现自己的系统调用，需要在 `kernel/syscall.h` 中定义该系统调用对应的 `syscall number`，然后在 `kernel/syscall.c` 的 syscalls 数组中把该系统调用加上，记得在数组前面的位置把该系统调用声明。

实际当进入系统调用后，执行某一系统调用的函数在 `kernel/sysproc.c` 中，sys_trace 函数要实现在该文件中。
xv6 中最初定义的 proc 结构体是没有 tracemask 这个参数的，所以需要在 proc 结构体的定义中加上。和进程相关的结构体、方法等定义在 kernel/proc.h 和 kernel/proc.c 文件中。此外，在 kernel/proc.c 文件的 fork() 函数中要多增加一下，fork 本来的功能是让父进程创建一个子进程，多增加一条把父进程的 tracemask 拷贝给子进程的 tracemask 的语句，这样，任意进程执行的时候，即使创建了子进程，子进程一样可以得到这个 tracemask 参数，然后看情况打印。
`syscall[num]()` 代表执行系统调用号为 `num` 的系统调用，然后把返回值写入到寄存器 `a0`里，为了能够打印指定的系统调用及结果，取出当前进程的 tracemask 然后跟执行的进程号 num 求与，结果非零时打印对应系统调用的进程号，系统调用名称和结果。

最后的最后，我参考的资料里这么说（虽然我自己没有遇到这个问题），trace 只希望在调用它的时候打印，而不希望正常的系统调用都打印，我参考的资料遇到了正常也打印的问题，他的解决方案是在 `kernel/proc.c` 文件中的 `freeproc` 函数里加一句，将 tracemask 置为 0.

## sysinfo
前面就照着 tips 里说的做，先把 sysinfo 系统调用的相关内容声明在系统调用各个文件中，可以参考前一个 trace 实验，注意系统调用函数的合理命名，否则调用的时候会出错。

完成 sysinfo 总共需要做三个步骤：

第一步 拷贝内核 sysinfo 到用户进程

用户进程调用系统调用的时候提供了一个参数：`sysinfo*` 其实只是一个指向用户进程虚拟地址的指针，里面并没有实际的东西存在，利用 `copyout()` 函数把真实的信息写入到这个 `sysinfo*` 的地址里，数据类型为 `struct sysinfo` 的指针。

关键在于 `copyout()` 的作用，它将内核的一些信息拷贝到用户进程中。`copyout()` 函数的用法参考了 `kernel/sysfile.c` 中的 `sys_fstat()` 函数和 `kernel/file.c` 中的 `filestat()` 函数。

知道了怎么把数据从内核拷贝到用户进程，接下来就是去获取真正需要的数据，总共需要两个数据：

1. 当前空闲的内存地址大小
2. 当前非 `UNUSED` 状态的进程数量

接下来分别在 `kernel/kalloc.c` 和 `kernel/proc.c` 写两个函数实现上述的两个功能，就可以在 `sysinfo` 系统调用中先调用上面两个函数获得真正的数据，然后再调用 `copyout` 写入用户进程中。

第二步 获取空闲内存地址大小

读了 RISC-V 的手册后得知：RISC-V 中内存地址以 page 的形式管理，每个 page 4096个字节，空闲的内存page 之间以空闲链表的形式存储，所以只需要用一个指针从遍历整个空闲链表计数，然后总数乘以单个 page 的大小，就是总的空闲内存大小。

第三步 获取非 UNUSED 进程数

观察进程的实现代码 proc.c 得知：进程管理在数组 proc 中，最大的进程数定义为 NPROC ，同样遍历整个数组，如果当前进程的状态不等于 UNUSED 就计数器++，最后返回。
