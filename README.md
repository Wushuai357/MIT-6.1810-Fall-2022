# MIT-6.1810-Fall-2022
# Lab: Networking

目标

为 xv6 中的 network interface card（NIC）写一个硬件驱动。

实现 `kernel/e1000.c` 中的 `e1000_transmit` 和 `e1000_recv` 函数，这样驱动能够收发网络报。

建议

1. xv6 (guest) 的 IP 地址为：`10.0.2.15` ，qemu 模拟的主机的 IP 地址为：`10.0.2.2`。
2. Makefile 将发送和接受的文件都记录在 `packets.pcap` 文件中，可以通过指令：**`tcpdump -XXnr packets.pcap`** 查看确定是否正常。
3. `kernel/e1000.c` 包含了初始化网卡 E1000 的代码，以及**需要补充**的发送和接收包的函数。`e1000_init` 函数配置 E1000 从内存中读取要发送的数据和将接收到的数据写入内存中，这样不通过 CPU 而直接从内存读写的硬件设备被称为 DMA（Direct Memory Access）。
4. 
5. `kernel/e1000_dev.h` 定义了 E1000 标准中寄存器和位标志含义。
6. `kernel/net.c` 和 `kernel/net.h` 包含了一个简单的实现 IP 、UDP、ARP 的网络协议栈，此外，还包含了一个灵活的数据结构可以保存包，被称为 `mbuf`。
7. `kernel/pci.c` 中的代码在 xv6 启动的时候搜寻 E1000 网卡。

