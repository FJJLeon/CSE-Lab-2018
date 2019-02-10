## HDFS and Fault Tolerance
使用了腾讯云的4个云服务器，实现使用HDFS，实现心跳检查、冗余和容错

[文档](https://ipads.se.sjtu.edu.cn/courses/cse/labs/lab4.html)肉多多NB

### Log
配置环境：
1. 腾讯云买4个入门配置服务器
2. 进行SSH登陆
    a. 下载putty https://www.chiark.greenend.org.uk/~sgtatham/putty/latest.html
    b. 生成SSH key 并绑定到云主机 https://console.cloud.tencent.com/cvm/sshkey
    c. 根据文档用putty SSH登陆登陆 https://cloud.tencent.com/document/product/213/5435
        i. 注意 这里登陆的用户需要有root权限  不对啊，CSE用户无法用SSH登陆
        Allow "cse" to sudo
        Which users could use sudo is controlled by /etc/sudoers. You can use sudo visudo to edit it. Just add cse ALL=(ALL:ALL) NOPASSWD: ALL at the end of the file. This will allow user "cse" to use sudo without password.
3. 修改实例名和主机名（Honstname），app、name、data1、data2
    a. 实例名是腾讯云console 显示的名字，最好与实际的hostname一致
    b. 修改主机名
        i. 登陆，sudo vi /etc/hostname  修改为对应的hostname，保存
        ii. 重启机器
        iii. 会由一个随机分配的主机名变成 
        iv. `ubuntu@app:~$`
4. 添加一个新的用户 cse区别于原本默认的 ubuntu
    a. sudo adduser cse
    b. Allow cse to sudo
    Which users could use sudo is controlled by /etc/sudoers. You can use sudo visudo to edit it. Just add cse ALL=(ALL:ALL) NOPASSWD: ALL at the end of the file. This will allow user "cse" to use sudo without password.
    不对啊，CSE用户无法用SSH登陆
5. 修改每个VM的/etc/hosts，设置4个VM private-IP对hostname的映射
    a. 循环
6. 似乎最开始的时候 root是没有密码的，且ubuntu作为sudo权限可以
    a. 使用 sodo passwd root 设置 root的密码，就可以进入root了
7. 修改了hostname之后sudo时会出现 sudo: unable to resolve host data1
    a. 需要修改 /etc/hosts， 把原来随机分配的hostname改为对应的
8. 看上去在ssh别的云主机时  Permission denied (publickey)的原因是SSH的公私密匙被拒绝, 需要 `vi /etc/ssh/sshd_config` 设置 `PasswordAuthentication yes`
9. 公钥复制
    a. 手动复制到远程机的公匙有换行符，是不对的
    b. 使用 ssh-copy-id -i .ssh/id_rsa.pub xxx(远程机，若host没指定为 xx@xx.xx.xx)
    c. https://blog.csdn.net/liu_qingbo/article/details/78383892
10. 把rododo的public放到 app的~/.ssh/authorized_keys里面，注意上面这个（没问题）
11. 用sudo service ssh status 查看本机ssh被连接的状态
12. 不在局域网内的SSH连接
    a. 设置 vi /etc/hosts 设置公网IP与对应hostname的映射
    b. 配置 ~/.ssh/config，形如
    ```
    Host xxx
    user yyy
    Host app
    user cse
    ```
    注意H大写，u小写
    c. 用ssh-keygen和ssh-copy-id xxx 完成密匙配对
    d. 其实在不配对的时候就能ssh xxx，只不过需要用密码

13. app的 .ssh 中需要有文件
其中id_rsa是连接 name 、 data1、 data2用的
在 authorized_keys 中，要有
    •  从自己本地虚拟机连app的公钥
    • 从rododo连接app的公钥
    • 自己连自己的公钥，测试要用。。
其他（name、data1、data2）的 authorized_keys要有app连用的公钥
14. 一直重新打开docker是不对的
	应该用 **docker start** [names/container ID]重启
再 docker attach xxx 或者 docker exec -it xxx /bin/bash 进入
19. 为4各VM安装java，sudo apt-get install openjdk-8-jre-headless
20. 为4个VM安装hadoop， 
wget http://mirrors.hust.edu.cn/apache/hadoop/common/hadoop-2.8.5/hadoop-2.8.5.tar.gz
tar -xf hadoop-2.8.5.tar.gz
确保解压后的路径位置为 /home/cse
21. 写part1发现part0中的问题 
/etc/hosts 中的 127.0.0.1 后面的name（或者data1、data2）不要
namenode启动的时候会试图通过name这个名字确定自己监听哪个地址，解析到127.0.0.1的话他就会只监听本地的连接
22. VERBOSE=1 ./test-lab4-part1.sh  可以打log执行

* Part2 
23. 在extent_protocol中公开更多inode级接口
24. LocatedBlock 是啥子？包括
    a. block id
    b. 在整个文件的block offset（？）
    c. block size，最后一个block的size需要注意，可能not full
    d. Block location，在part3使用，有replica datanode，p2就是master_datanode
25. 打自己的log，用printf的时候加上 fflush(stdout)
26. create完了就拿到了lock，要call complete才释放lock
27. 呃呃呃，unlink操作比想的复杂，除了移除，还直接把文件删除了，所以会把后面那个分配过去

* Part3
28. namenode里的master node不需要存入数据结构，不需要在getDN的时候返回
29. 心跳检测，使用master_datanode的heartbeat作为基准（注意这是个never died）
需要记录每一个其他replic datanode的heartbeat值
当发现master datanode发送heartbeat时，用这个master_hb与其他的比较，若相差大于一定值则detect dead
注意，当新的datanode register时，应当用master_hb初始化其计数值
30. 不知道为什么，在inode_manager里 append_block里面需要同时update inode blokc size，否则不行



