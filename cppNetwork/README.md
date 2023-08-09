interface.h             #一些可以复用的基础组件，慢慢补充



socket.cpp              #简单的socket服务器程序


#IO复用

select.cpp              #简单应用select的服务器代码
        使用TCP测试工具收发包，将收到的数据包原路返回，测试了同时使用3条TCP连接，任意发送断开连接功能正常。

poll.cpp                #简单的poll服务器代码

epoll.cpp               #简单的epoll服务器代码

写完网络服务器和简单的IO复用之后的一些笔记：
        1、socket初始化，第一个参数：  ， 第二个参数 ，非阻塞IO的使用.
        2、bind IP\port绑定，family类型
        3、listen监听：backlog：指定连接队列的最大长度，即在未accept之前能够等待连接的最大数量
        4、epoll用到的关键字：struct epoll_event、epoll_wait、epoll_ctl、
        5、ev.events取值
        6、epoll_create();  参数
        7、epoll_wait();    最后一个参数
        8、epoll_ctl();     第二个参数的选择：EPOLL_CTL_ADD/EPOLL_CTL_DEL/EPOLL_CTL_MOD
        9、select();        函数参数的含义
        10、select用到的关键字：fd_set、FD_ZERO、FD_SET、select、FD_ISSET、FD_CLR
        11、poll();      第三个参数的作用？
        12、poll用到的关键字：struct pollfd、poll


reactor.cpp             #基于epoll的端口复用reactor框架
        1、虽然用的是.cpp文件，但还是用的C语言写法，简单的把recv到的数据原路send回去
        2、接下来给reactor代码加注释
        3、封装为C++代码库，并快速构建高并发服务器，做简单的TCP收发数据（将收到的数据翻转后原路返回）
    数据结构的设计：
![Alt text](reactor_dataStruct.png)

demo.cpp                #基于reactor框架封装应用的高并发服务器
