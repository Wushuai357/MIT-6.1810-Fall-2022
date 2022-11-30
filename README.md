# MIT-6.1810-Fall-2022

# Lab: File System

### Large files
目标

当前 xv6 能支持的最大文件是 268 个 blocks，每一个 block 大小为 1 KB，因为每一个 file 的 struct 中有 12 + 1 个指针，前 12 个指针代表 12 个blocks，最后一个指针是一个间接指针，它指向一个 block ，其中存放的全部都是指针，每个指针又指向一个存放数据的 block，一个指针大小为 4 字节，所以一个 block 总共可以放 256 个指针。总计 12 + 256 = 268 个 blocks。

`bigfile` 测试命令会尝试创建一个 65803 blocks 大小的文件，修改 xv6 中代表文件的结构体，实现这一功能。

原理

将 file struct 中的一个指针替换成一个二级指针，即它指向一个存放指针的 block，该 block 内存放的也全是指针，每一个指针又指向一个存放指针的 block，这一级 block 才指针数据block，则该二级指针总共可以存放：256 * 256 个 block，最后再加上原来结构中剩余的 10个 block 以及一个间接指针代表的 256 个block，总计：256 * 256 + 256 + 11 = 65803 个 blocks。
提示

1. xv6 文件系统总的 blocks 数定义在 `kernel/param.h` 中的 `FSSIZE` 变量中，编译的时候应该有下面这句输出：`nmeta 70 (boot, super, log blocks 30 inode blocks 13, bitmap blocks 25) blocks 199930 total 200000` 。这代表当前 xv6 编译的文件系统，有 70 个 metadata blocks，199930 个 data blocks ，总计 20，0000 个 blocks。
2. 磁盘上的文件结构定义在 `kernel/fs.h` 的 `struct dinode` 中，着重关心其中的：`NDIRECT`、`NINDIRECT`、`MAXFILE` 和 `addrs[]` 变量。
3. `fs.c` 中的 `bmap()` 函数查找某一文件在磁盘上的数据，当读/写文件的时候都会调用该函数，当写入的时候它在有需要的时候分配新的 blocks 存放文件数据，有需要时也会分配一个间接引用的 block 来存放数据 block 指针。`bmap` 接收两种 block 号，一种是逻辑 block 号 `bn` ，代表该 block 属于某一个内存中文件的结构体中的索引；第二种是 `ip→addrs[]` 中的 `addr` ，也是传给 `bread` 的参数，代表磁盘中的 block 号。bmap 实现从文件逻辑 block 号到磁盘 block 号的映射。
4. 修改 `struct inode` ，前 11 个直接指向数据 block，第 12 个指向一个 singly-indirect block，第 13 个指向一个 doubly-indirect block。
5. 确保你知道 `bmap` 的原理，画一张图来描述 ip→addrs[] 、singly-indirect block 、doubly-indirect block 及它指向的 singly-indirect block 以及 data blocks 之间的关系。
6. 思考如何索引 doubly-indirect block 及它指向的 singly-indirect block 。
7. 如果要修改代表直接引用数据 blocks 的指针数量的 `NDIRECT` ，必须修改 `file.h` 中 `struct inode` 对于 `addrs[]` 的声明，确保 `struct inode` 和 `struct dinode` 中的 `addrs[]` 数组有相同的数量。
8. 如果要修改代表直接引用数据 blocks 的指针数量的 `NDIRECT` ，确保创建一个新的 **fs.img** ，因为编译文件系统的指令 `mkfs` 使用 `NDIRECT` 来创建文件系统。如果文件系统编译错误，就可以直接删掉这个新的 fs.img 文件，make 指令会创建一个新的、干净的、正确的文件系统。
9. 对于每一个 `bread` 的 block 记得要 `brelse` 释放它。
10. 类似初始的 `bmap` 函数，只有当有需要的时候才会分配 一级和二级间接引用的 block。
11. 确保 `itrunc` 会释放包括 二级间接引用之内的所有文件blocks。
12. 当通过 `bigfile` 和 `usertests -q` 测试则代表成功，`bigfile` 至少需要一分半来执行，`usertests` 也需要更多的时间来执行。

思路

第一步，修改 `NDIRECT` 的值：

1. 修改 `NDIRECT` 值，从 12 缩减到 11 。代表前 11 个直接引用数据 blocks 的指针。
2. 修改 `file.h` 中 `struct inode` 对于 `addrs[]` 的声明：从 `NDIRECT + 1` 改成 `NDIRECT + 2` 。因为 `NDIRECT` 减少 1 要保持总量不变。
3. 类似的，修改 `fs.h` 中 `struct dinode` 中的声明。

第二步，定义文件 block 逻辑索引 `bn` 的范围：

1. 定义 `NDIRECT = 11` ，代表前 0～10 总计 11 个数据 blocks
2. 定义 `SINDIRECT = (BSIZE / sizeof(uint))` ，代表第 11～ 267 总计 256 个一级间接引用的 blocks
3. 定义 `DINDIRECT = (BSIZE / sizeof(uint)) * (BSIZE / sizeof(uint))`，代表第 267 ～ 65803 总计 65536 个数据 blocks，其中，bn / (BSIZE / sizeof(uint)) 代表它在第一级引用值，bn % (BSIZE / sizeof(uint)) 代表它在第二级引用的值，指向数据 block。
4. 修改代表文件最大 bn 值的 `MAXFILE = 以上三个值的和`

第三步，修改 `bmap` 函数，`bmap` 函数返回 inode 结构体中地址为 bn 的 blockno 值，根据 `bn` 值的不同分为三个部分对应于三种情况：

1. `if(bn < NDIRECT)` 对于前 11 个 blocks 的访问不用修改，修改 `NDIRECT` 值后的操作都是正确的
    1. 先判断 inode 结构体中 索引为 bn 的 blockno 是否为 0，如果是说明未使用，先调用 balloc 为它分配一个 block，然后把 blockno 填入 bn 对应的数组中。
    2. 返回这个 blockno 值。
2. `if (bn < SINDIRECT)` ，之前先 `bn -= NDIRECT` ，这一步的操作把 bn 变成了在 singly-indirect block 中的索引值（偏移量）。
    1. 先判断 inode 结构体中 索引为 `NDIRECT` 的 blockno 是否为 0，如果是说明未使用，先调用 balloc 为它分配一个 block，然后把 blockno = addr 填入 `SINDIRECT` 对应的数组中。
    2. 然后用 `bread` 函数把 addr 里的数据读进内存的缓存区中，存放在一个 `struct buf*` 中，用一个 `uint* a` 指向这个 buf 中的数据，此时，a[bn] 代表的就是偏移量为 bn 的结构体的值，这里有一个很巧妙的地方，bn 代表的是 `struct inode` 指针，而按理来说 `uint* a` 应该以字节为索引，妙就妙在，a 是一个 uint* 的指针，所以 a + 1 不是偏移一个字节，而是偏移 1 个指针也就是 4 个字节，所以可以直接用 a[bn] 的方式来定位到索引为 bn 的 in do
    3. 然后就还是老规矩，如果 `a[bn]` == 0 说明此时还是空的，为它分配一个 block ，把 blockno 写进这个地址，额外需要一步操作就是 `log_write` 这个 buf，因为我们现在只写入了缓存中而没有真的写入文件中，所以需要专门写入。
    4. 写入完了之后要调用 `brelse` 函数释放前面拿到的 buf。
    5. 最后返回 blockno 值。
3. `if (bn < DINDIRECT)` ，之前先 `bn -= SINDIRECT` ，这一步操作把 bn 变成了在 doubly-indirect block 中的索引，其中，`bn / (BSIZE / sizeof(uint))`  代表第一级索引，`bn % (BSIZE / sizeof(uint))` 代表第二级索引：
    1. 老规矩，先判断 `NDIRECT + 1` 的 blockno 是否为 0， 如果是说明未使用，先调用 balloc 为它分配一个 block，然后把 blockno = addr 填入 `NDIRECT + 1` 对应的数组中。
    2. 读入 addr 对应的 block 到缓存中，继续用 a 表示它里面的数据，判断 `a[bn / (BSIZE / sizeof(uint))]` 是否为 0，如果是说明未使用，先调用 balloc 为它分配一个 block，然后把 blockno = addr 填入对应的位置，调用 `log_write` 写回这个 buf。写入完了之后要调用 `brelse` 函数释放前面拿到的 buf。
    3. 然后把新的 addr 对应的 block 读取到缓存中，继续用 a 表示它里面的数据，判断 `a[bn % (BSIZE / sizeof(uint))]` 是否为 0，如果是说明未使用，先调用 balloc 为它分配一个 block，然后把 blockno = addr 填入对应的位置，调用 `log_write` 写回这个 buf。写入完了之后要调用 `brelse` 函数释放前面拿到的 buf。
    4. 最后返回第三次拿到的 `addr` 。
4. 在 itrunc 函数中也是类似的，从第二层先开始释放，然后释放第一层，最后释放顶层的 `NDIRECT + 1` 。
5. 额外的，在 `mkfs.c` 中使用了最初的变量 `NINDIRECT` ，改成新定义的 `SINDIRECT`。

### Symbolic links
目标

在 xv6 中增加 soft link（symbolic link）功能，实现系统调用：

`symlink(char *target, char *path);`

它在 path 指定的路径创建一个 symbolic link 指向 `target` 指向的文件。

基础知识

> 什么是软链接
> 

软链接类似于快捷方式，它指向路径 target 所指向的源文件，相较于源文件有不同的 inode，存放的内容是路径 target 本身。

当 target 文件被删除、移动、重命名后，软链接还存在，只是因为指向的文件不存在了所以失效了。

当软链接的文件被删除后，源文件不会受到影响。

软链接一般实现在 inode 结构体中。

> 什么是硬链接
> 

硬链接是多个进程的目录项指向同一个 inode，它们的 inode 值相同。

只有当所有指向该文件的硬链接以及文件本身都被删除之后，该文件才会真的被删除，否则盲猜只是 `—ref`。

硬链接一般实现为目录中的一条表项，它们有自己的文件名，但指向同一个 inode。因此，每一个打开文件都至少应该有一个硬链接。

> 软链接和硬链接有什么区别
> 
- 软链接和指向的文件 inode 值不同，硬链接的 inode 相同
- 源文件删除后软链接失效，硬链接删除则当且仅当所有指向源文件的硬链接及源文件本身都被删除之后才会被删除
- 软链接以文件路径的方式指向源文件，所以可以跨文件系统；硬链接以相同的 inode 来指向源文件，所以不能跨文件系统（不同的文件系统有不同的 inode 表）
- 软链接可能指向一个无效的路径；硬链接指向的文件一定存在
- 软链接一般实现在 inode 结构体中；硬链接一般实现在目录的目录表中。

> 实现
> 

早期的 symbolic link 实现是实现一个文件，用文件的类型指明这是一个软链接文件，文件里存放的数据就是指向源文件的路径。但是，这样的实现方法访问速度又慢还对于磁盘空间小的系统很不友好。

后来改进的实现被称为 fast symlinks，允许将指向源文件的路径存放在 inode 的数据结构中，

提示

1. 查看 man page of symlink 获取更详细的信息。
2. 将 `symlinktest` 添加成系统调用然后在系统中进行测试，此外，还需要通过 `usertests -q` 测试。
3. 实现系统调用函数 `symlink(target, path)` 在 path 处创建一个新的 symbolic link 指向 target，target 指向的文件未必必须存在，可以将 target 地址存在 inode 的结构体的数据块中。如果成功应该返回 0，失败则返回 -1.
4. 修改 `open` 函数，当它发现要打开的文件类型是 symbolic link 且传入的参数在位中指定了 `O_NOFOLLOW` 时，直接转而去打开 inode 数据块中存放的 target 地址代表的文件，如果该文件不存在，返回 -1。如果 target 地址指向的文件依然是一个 symbolic link，必须递归地打开它直到打开一个非软连接的文件。为了防止回路的情况，在连续递归地打开10个软链接文件后，直接返回一个 error code。
5. 其他的系统调用比如 link 和 unlink ，则保持原样，所有的操作都针对于软链接文件而不是软链接指向的文件。

思路

第一步，将 `symlinktest` 添加成系统调用：

1. 在 `user/usys.pl` 中为它添加入口
2. 在 `kernel/syscall.h` 中为它分配新的系统调用号
3. 在 `kernel/syscall.c` 中添加对该函数的系统调用
4. 在 `user/user.h` 中声明该系统调用函数
5. 将该函数 `sys_symlink` 实现在 `kernel/sysfile.c` 中

第二步，维护与软链接相关的标志：

1. 在 `kernel/stat.h` 中定义一个 `T_SYMLINK` 常量，如果某一文件类型等于该值，说明它是一个软链接。
2. 在 `kernel/fcntl.h` 中添加一个位标志 `O_NOFOLLOW` ，`open` 系统调用可以使用它。注意不要覆盖原有的位标志。

第三步，实现 `symlink` 系统调用函数：`symlink(char *target, char *path)`

1. 调用 `create` 函数，传入的参数为 `create(path, T_SYMLINK, 0, 0)` ，它实现了以下三大功能：
    1. 防御性编程，先判断 `path` 所指的路径是否有效，`path` 所代表的文件名是否被使用。如果无效或者同一目录下已有文件使用该名称， `return 0` 。对于 symlink 系统调用来说，因为 `target` 的地址未必有效，将来也可能会失效，所以无需判断其有效性。
    2. 创建 `struct inode*` 节点 ip。
    3. 将创建的 `struct inode*`  ip 更新到 path 所指的父目录数据项中。
2. 拿到返回的 ip，继续调用 `writei` （kernel/fs.c）函数，将 target 路径值写入 ip 的数据项中：`writei(ip, 0, target, 0, sizeof(target))` ，因为是系统调用，在内核中处理，所以这里第二个参数填 0，又因为 ip 是新建的 inode 指针，它里面的值都是空的，所以第一次的 offset 就从 0 开始。
3. 如果成功就返回 0，如果期间遇到失败，返回 -1.

第四步，在 `sys_open` 系统调用，增加遇到打开软链接文件时的处理方式：

1. 如果打开操作的权限不包含 `O_CREATE` 再处理，因为访问软链接时不可能被赋予这个权限。
2. 写一个重复 10 次的循环，每一次获取当前的 path 对应的 inode 节点指针，经历一系列对该 ip 类型的判断，如果都通过了说明还是一个软链接，就进入下一轮循环，直到某一次取到的 inode 不是软链接，或者 10 次循环结束，按照 TIPS 中的建议，直接判断是一个回路，错误返回。
3. 后续处理就保持 open 原来的操作流程。
