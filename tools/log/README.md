#
    主要内容：
        1、日志写入逻辑
        2、Log4cpp日志框架
        3、Log4cpp范例
        4、muduo日志库
        5、spdlog简要分析

#
    1、日志写入逻辑
        关于write：
            1、fwrite   C语言应用层接口
            2、write    linux系统接口
        写入逻辑：
            1、 用户程序     -- fwrite -->     用户缓冲区（setBuf()）        -- fflush() / write() -->       内核缓冲区      -- fsync()  -->     stdout 
            2、 stdin       -- 页面宽度 -->     内核缓冲区      -- read()  -->      用户缓冲区（setBuf()）     -- fread() -->       用户程序
            3、调用fwrite的时候，写入用户缓冲区，攒一些数据之后调用内核write将日志写入到文件.
            4、数据没有及时写入程序崩溃了，日志没有被写入磁盘，需要开启coredump查看内存数据dump.
    
    2、log4cpp日志框架
        1、适合用在客户端，不适合使用在服务器批量写入数据.
        

        