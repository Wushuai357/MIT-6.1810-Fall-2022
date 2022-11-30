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
