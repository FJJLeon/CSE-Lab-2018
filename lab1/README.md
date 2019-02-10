## Lab 1: Basic File System
一个基于Inode的Unix文件系统
[文档](https://ipads.se.sjtu.edu.cn/courses/cse/labs/Lab1.pdf)(可能会过期)

### Log
* Part1
1. Inode num = 1024, but inode inum in 0~1023
2. But why inode for root_dir = 1?
3. Sizeof(struct inode) = 424
4. 一共就1024*1024*16 bytes 硬盘空间，即32768个block，每个bitmap block有4096个bits可以用来标识4096个block是否被使用，则bitmap block需要8个
5. #define IBLOCK(i, nblocks)     ((nblocks)/BPB + (i)/IPB + 3), 给出表示储存inode所用的block的id，第1个inode在第12个block，第1024个inode在1035个block
6. #define BBLOCK(b) ((b)/BPB + 2), 给出表示第b个block在bitmap中的bit所在的block的id，需要+2，根据ppt，前面有boot block（0） 和 super block（1），也即0、1用于前面，2、3、4、5、6、7、8、9用于bitmap block？但是这样也不能到 inode 1在block 12啊。。
7. Bitmap 的第一个在id=2的block
8. Indirect，开100的direct可以不写indirect过test

* Part2
1. 在inode_manager::write_file(uint32_t inum, const char *buf, int size)的参数中 存在strlen(buf) 不等于 size的输入，或者说buf中有\0？，不能检测
2. 解决所有warning比较好
3. 每次重新进入dacker，都是新的环境，需要加入 stu到sudoer？
    a. Su （pwd：000）进入root
    b. Vim /etc/sudoers
    c. 加入一行  stu ALL=(ALL) ALL
    d. Exit
不需要，查看docker命令，容器并没有销毁，使用docker start可以重新启动
4. extent_protocol 在 symbolic 时要加 symlink的类型



