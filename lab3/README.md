## Cache for Locks  
对加锁服务设计并实现Cache策略, 减少client拿锁的次数

[文档](https://ipads.se.sjtu.edu.cn/courses/cse/labs/Lab3.html)

### Log
* Review docker usage
    ```
    % mkdir lab-cse 
    % cd lab-cse
    % git clone http://ipads.se.sjtu.edu.cn:1312/lab/cse-2018-fall.git lab1 -b lab1 
    % cd lab1 
    % git checkout lab1 
    % docker pull ddnirvana/cselab_env:latest 
        # suppose the absoulte path of lab-cse is /home/xx/lab-cse 
    % sudo docker run -it --privileged --cap-add=ALL -v /home/junjie/lab-cse:/home/stu/devlop ddnirvana/cselab_env:latest /bin/bash 
        # now you will enter in a container environment, the codes you downloaded in lab-cse will apper in /home/stu/devlop in the container 
    % cd /home/stu/devlop 
    % make
    ```
* 进入同一个docker容器
1. 使用attach：docker attach [ID]
ID是先前开启的终端后的@后的hex串,可以用 `docker ps -a`查看
2. 使用 docker exec -it [NAMES]or[ID] bash

* Lab3
1. lock_client_cache.{cc,h}构造函数中
    a. 使用hostname和随机选取的rlock_port构造一个id字符串，在要锁时应发送给server，替代cl->id()
    b. 同时其还在lock_client_cache中新建了一个rpcs(server)，叫做 rlsrpc，监听上述端口，同时注册了两个handler函数（revoke、retry）来响应server的rpc
2. 对每一把锁（每一个inode？）都会维护一些状态以表示其
    a. 对于server，需要维护该锁是否被某个client拥有
    b. 对于client，需要维护某锁是否被自己拥有，是否被自己的某线程使用
3. RPC call 的参数传递顺序！！
4. 使用pthread_self()可以获取线程的tid，如果要用的话
5. 不要拿着互斥锁发送RPC，说明mutex是要用的
6. 行为：
    a. Client A请求锁
        i. 锁是free，则server授予锁并返回OK
        ii. 锁被其他client B持有，且有其他client已经在等待，返回RETRY（这是一个返回值），意味着revoke rpc已经发送给持有锁的线程过了
        iii. 锁被其他client B持有，但没有其他client在等待，发送revoke rpc给持有锁的client B，使得B准备释放锁（可能仍在使用则等待使用完成）回server，server重新拿到B释放的锁后向client A发送 retry rpc（这是一个rpc），将锁授予A并返回OK
    b. rlock_protocol中已经定义了client需要响应server的rpc，即revoke和retry
    c. 对于Server需要设置几个状态给每个锁
    需要设置状态表，需要包括1.是否被授予  2. 授予给了谁 ID  3. 等待着的client的set
        i. FREE：空闲，没有授予给client！！此状态仅出现在revoke后，retry完成前
        ii. GRANTED：授予给了某各client。note：同时应该保存状态表示 该锁被哪个client锁持有，可以用std::string id表征
        iii. REVOKING：已经发送revoke rpc要求持有锁的client尽快释放锁了（没有thread用则立即，否则那个thread用完）
        iv. 还要吗
    d. 对于client需要设置几个状态，需要设置状态表
        i. NONE：client不知道这个锁什么情况，要么在server那，要么在别的client那
        ii. FREE：client拥有这个锁，但是锁没有被某个线程使用
        iii. LOCKED：client拥有这个锁，但是锁被某个线程锁使用.note：如何表示被某个线程所使用
        iv. ACQUIRING：client正在向server要锁，还没要到？
        v. RELEASING：client正在向server释放锁，还没放完？revoke过程中
    e. 一个retry流程：
        i. 首先 client A拿着锁。
        ii. client B要锁，发送acquire RPC给server，server在GRANTED分支先设置 B 为next即下一个拿锁的，设置锁的状态为REVOKING，然后（释放mutex，可以响应其他clt，若还是这个锁，以RETRY响应）用handle连接 A， 成功后发送revoke RPC给A。
        iii. A用revoke_handler响应，（若A有线程正在使用该锁，则wait等待release来唤醒），设置锁状态为 REVOKING，发送release RPC给server
        iv. server以release来响应A，设置该锁为FREE（即没人使用，  该状态仅出现在revoke 发送rpc给server之后，server发送的retry给B返回之前），发生retry RPC 给B
        v. B 以 retry_handler响应，再次发送acquire给server，此时server 发现锁是 FREE，改为GRANTED，且被B所拥有
        vi. 第二次acquire返回给B，B的retry返回给server，server的release返回给A， A的revoke返回给server，server第一个acquire返回给B以OK，B的acquire返回，得到锁
        vii. ！！！！！v中真的要call B retry？
            1) A：需要call B的retry
            2) 但是A的revoke不必须在revoke中（给server发release），可以记录一个状态量，revoke来过，然后在release或者什么时候进行call release
7. 问题
    a. client内部某个thread X等待锁A，因为锁A被 thread Y锁使用，在Y free 锁后，通知X来拿锁，但是此时如果server发送revoke RPC要求该client release这个锁？？？？？
    b. 在client把 NONE和RELEASING    LOCKED和ACQUIRING  的执行放在一起没毛病吗？好像没有
    c. lock_server_cache如何发送revoke&retry rpc 给 lock_client_cache？？？See Handle
    d. 什么时候发送retry RPC 给client呢？？server接到先前拿着锁的clt发了release之后
    e. 收到retry RPC的client必然可以且必须要得到指定的锁

