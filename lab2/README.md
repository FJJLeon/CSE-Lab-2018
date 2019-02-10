## Lab-2: RPC and Lock Server
将文件系统的调用变成远程调用，并实现控制并发的加锁服务

[文档](https://ipads.se.sjtu.edu.cn/courses/cse/labs/Lab-2.html)

### Log
1. 需要 git pull merge 然后处理 conflict（推荐vscode有处理的可视化），直至make无error
此时./grade.sh 应该是0分的，且lab1-part2-a 错误
因为 extent_client.cc的接口需要用rpc重写
2. 出现了神奇的bug。。启动grade时会计数 yfs_client 和 extent_server
原因是：经过了很多次的 ./grade.sh ./start.sh 和手动删除 yfs1和yfs2文件夹后！！！出现了多个僵死进程：
![僵死进程](https://github.com/FJJLeon/CSE-Lab-2018/tree/master/lab2/Lab-2_files/dead.jpg)
导致过不了检查是因为多了一个extent_server 进程
3. symlink在解除的时候用的也是 unlink，所以在其中判断name所对应的inode number时
不能判断 !isfile(ino) 即认为是 目录而不给操作，也可能是 symlink
故改为 isdir(ino) 时 不给操作
4. Lock_demo 过不了  10/10midnight
ps：原因在于 docker中的进程 在另一个terminal是不可见的
solution：直接在两个terminal中跑./lock_sever和./lock_demo
或者想办法使得两个terminal进入同一个docker(使用docker exec)
5. lock作用于yfs_client时要注意内部调用链同时想要获取一个锁时出现的死锁
ex：create（acquire）->lookup -> isdir（acquire（dead））
6. createORmkdir中的acquire应该在lookup之前，否则可能出现错误
7. 命令行重复执行
    ```
    for i in {1..10}; do ./test-lab2-part2-a ./yfs1 ./yfs2; done;
    ```