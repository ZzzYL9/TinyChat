#include<iostream>
#include<fcntl.h>
#include "global.h"
using namespace std;


//listen的backlog大小
#define LISTENQ 200
//监听端口号
#define SERV_PORT 8000
#define INFTIM 1000 

// extern void handle_all_request(string epoll_str,int conn_num,int epollfd);
extern void* handle_all_request(void *arg);
extern unordered_map<string,int> name_sock_map;//记录名字和套接字描述符
//extern clock_t begin_clock;//开始时间，用于性能测试，有bug
extern double total_time;//线程池处理任务的总时间
//extern time_point<system_clock> begin_clock;//开始时间，压力测试
extern int total_handle;//总处理请求数，用于性能测试
extern int total_recv_request;//接收到的请求总数，性能测试
extern int Bloom_Filter_bitmap[1000000];//布隆过滤器所用的bitmap
extern queue<int> mission_queue;//任务队列
extern int mission_num;//任务队列中的任务数量
extern pthread_cond_t mission_cond;//线程池所需的条件变量
extern pthread_spinlock_t name_mutex;//互斥锁，锁住需要修改name_sock_map的临界区
extern pthread_spinlock_t from_mutex;//互斥锁，锁住修改from_to_map的临界区
extern pthread_spinlock_t group_mutex;//互斥锁，锁住修改group_map的临界区
extern pthread_mutex_t queue_mutex;//互斥锁，锁住修改任务队列的临界区
extern int epollfd;
extern pthread_spinlock_t count_mutex;



//将参数文件描述符设为非阻塞
void setNonblok(int& sock){
    //获取文件描述符的当前标志值
    int ret = fcntl(sock, F_GETFL);
    if(ret < 0){
        perror("fcntl(sock, F_GETFL) error!");
        exit(1);
    }
    ret = ret | O_NONBLOCK; //设置为非阻塞模式
    //将更新后的文件状态标志位设置回套接字sock
    if(fcntl(sock, F_SETFL, ret) < 0){
        perror("fcntl(sock,SETFL,opts) error");
        exit(1);
    }
}

int main()
{
    pthread_spin_init(&name_mutex, 0);
    pthread_spin_init(&group_mutex, 0);
    pthread_mutex_init(&queue_mutex, 0);
    pthread_spin_init(&count_mutex, 0);
    pthread_spin_init(&from_mutex, 0);
    pthread_cond_init(&mission_cond, NULL);

    int i, maxi, listenfd, connfd, sockfd, epfd, nfds;
    ssize_t n;
    socklen_t clilen;
    struct epoll_event ev, events[10000];
    epfd = epoll_create(10000);
    epollfd = epfd;  //赋给全局变量，线程处理完事件后需要重新注册
    struct sockaddr_in clientaddr;
    struct sockaddr_in serveraddr;
    listenfd = socket(PF_INET, SOCK_STREAM, 0);
    setNonblok(listenfd);
    ev.data.fd = listenfd;
    ev.events = EPOLLIN | EPOLLET;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);
    bzero(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serveraddr.sin_port = htons(8023);
    bind(listenfd, (sockaddr*)&serveraddr, sizeof(serveraddr));
    listen(listenfd, LISTENQ);
    clilen = sizeof(clientaddr);
    maxi = 0;

    cout<<"准备连数据库\n";

    //连接MYSQL数据库
    MYSQL *con = mysql_init(NULL);
    mysql_real_connect(con, "127.0.0.1", "root", "root", "test_connect", 0, NULL, CLIENT_MULTI_STATEMENTS);
    string search = "SELECT * FROM user;";
    auto search_res = mysql_query(con,search.c_str());
    auto result = mysql_store_result(con);
    int row;
    if(result)
        row = mysql_num_rows(result);//获取行数

    cout << "连接数据库成功" << endl;

    pthread_t tid[1000];
    for(int i = 0; i < 1000; i++)
        pthread_create(&tid[i], NULL, handle_all_request, NULL);


    while (1) {
        
        cout << "--------------------------" << endl;
        cout << "等待事件的发生" << endl;
        nfds = epoll_wait(epfd, events, 10000, -1);
        cout << "事件发生，处理中..." << endl;
        for (i = 0; i < nfds; ++i) {
            if (events[i].data.fd == listenfd) {
                connfd = accept(listenfd, (sockaddr*)&clientaddr, &clilen);
                if (connfd < 0) {
                    perror("connfd<0");
                    exit(1);
                }
                else {
                    cout << "用户" << inet_ntoa(clientaddr.sin_addr) << "正在连接\n";
                }
                setNonblok(connfd);

                //设置用于注册的读操作事件，采用ET边缘触发，为防止多个线程处理同一socket而使用EPOLLONESHOT  
                //EPOLLIN：关注可读事件，当有数据可读时触发操作
                //EPOLLONESHOT：只会触发一次事件，确保每个事件只由一个线程处理
                // ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
                ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
                //设置用于读操作的文件描述符  
                ev.data.fd = connfd;                
                
                if (epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev) == -1) {
                    perror("epoll_ctl: client_fd");
                    exit(EXIT_FAILURE);
                }
            }
            //接收到读事件
            else if (events[i].events & EPOLLIN) {
                total_recv_request++;
                sockfd = events[i].data.fd;
                events[i].data.fd = -1;
                cout << "接收到读事件" << endl;

                pthread_mutex_lock(&queue_mutex);
                mission_queue.push(sockfd);
                pthread_cond_broadcast(&mission_cond);
                pthread_mutex_unlock(&queue_mutex);
            }
        }
    }
    close(listenfd);
}