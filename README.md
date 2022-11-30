# MIT-6.1810-Fall-2022

# Lab: System Calls

### using GDB
##### boot GDB
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

