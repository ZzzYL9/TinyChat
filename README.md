# TinyChat
使用epoll实现的多线程聊天系统



**注册：**

![image](https://github.com/ZzzYL9/TinyChat/blob/master/image/register.png)



**私聊：**

![image](https://github.com/ZzzYL9/TinyChat/blob/master/image/onlychat.png)



**群聊：**

![image](https://github.com/ZzzYL9/TinyChat/blob/master/image/groupchat.png)



**压力测试**：

测试思路：在客户端代码中添加计时器来测量程序性能指标，如请求每秒（QPS）等。

在本地创建一个account.txt文件，文件中保存一个有效的用户名和密码。每个线程将从文件中读取一对用户名和密码，并使用该信息建立连接发送登录请求。

在account.txt文件保存200行数据时，QPS大概有17290左右

![image](https://github.com/ZzzYL9/TinyChat/blob/master/image/QPS.png)
