#include "HandleServer.h"
#include "global.h"

extern unordered_map<string,int> name_sock_map;//名字和套接字描述符
extern unordered_map<int,set<int>> group_map;//记录群号和套接字描述符集合
extern unordered_map<string,string> from_to_map;//记录用户xx要向用户yy发送信息
// extern clock_t begin_clock;//开始时间，用于性能测试
// extern time_point<system_clock> begin_clock;
extern int total_handle;//总处理请求数，用于性能测试
extern int total_recv_request;//接收到的请求总数，性能测试
extern double top_speed;//峰值性能
extern queue<int> mission_queue;//任务队列
extern int mission_num;//任务队列中的任务数量
extern pthread_cond_t mission_cond;//线程池所需的条件变量
extern pthread_spinlock_t name_mutex;//互斥锁，锁住需要修改name_sock_map的临界区
extern pthread_spinlock_t group_mutex;//互斥锁，锁住修改group_map的临界区
extern pthread_spinlock_t from_mutex;//互斥锁，锁住修改from_to_map的临界区
extern pthread_mutex_t queue_mutex;//互斥锁，锁住修改任务队列的临界区
extern int epollfd;
extern pthread_spinlock_t count_mutex;


using namespace std;
using namespace chrono;

void* handle_all_request(void *arg){
    // char buf[1000];
    // memset(buf, 0, sizeof(buf));
    // int ret  = recv(con_fd, buf, sizeof(buf), 0);


    while(1){
        int queue_con_fd; //用来获取队列中的套接字描述符
        //取出任务
        pthread_mutex_lock(&queue_mutex);
        while(mission_queue.empty()){
            pthread_cond_wait(&mission_cond, &queue_mutex);//必须和queue_mutex配套使用
        }
        queue_con_fd = mission_queue.front();
        mission_queue.pop();
        pthread_mutex_unlock(&queue_mutex);

        time_point<system_clock> begin_clock= system_clock::now();

        int con_fd = queue_con_fd;
        cout << "con_fd:" << con_fd <<endl;
        int target_con_fd = -1; //聊天目标的套接字描述符
        char buffer[1000];
        string username, passwd;
        bool is_login = false; //记录当前服务对象是否登录成功
        string login_name; //记录当前服务对象的名字
        string target_name; //记录发送信息的目标名字
        int group_num; //记录群号

        //连接MYSQL数据库
        MYSQL *con=mysql_init(NULL);
        mysql_real_connect(con,"127.0.0.1","root","root","test_connect",0,NULL,CLIENT_MULTI_STATEMENTS);

        cout<<"-----------------------------\n";
        string recv_str;
        while(1){
            char buf[1000];
            memset(buf, 0, sizeof(buf));
            int ret  = recv(con_fd, buf, sizeof(buf), 0);
            if(ret < 0){
                cout<<"recv返回值小于0"<<endl;
                //对于非阻塞IO，下面的事件成立标识数据已经全部读取完毕
                if((errno == EAGAIN) || (errno == EWOULDBLOCK)){
                    printf("数据读取完毕\n");
                    cout<<"接收到的完整内容为："<<recv_str<<endl;
                    cout<<"开始处理事件"<<endl;
                    break;
                }
                cout<<"errno:"<<errno<<endl;
                close(con_fd);
                
                return NULL;
            }
            else if(ret == 0){
                cout<<"recv返回值为0"<<endl;
                close(con_fd);
                
                return NULL;
            }
            else{
                printf("接收到内容如下: %s\n",buf);
                string tmp(buf);
                recv_str+=tmp;
            }
        }
        string str = recv_str;

        //处理登录
        if(str.find("login") != str.npos){
            int p1 = str.find("login"),p2 = str.find("pass:");
            username = str.substr(p1 + 5, p2 - 5); 
            passwd = str.substr(p2 + 5, str.length() - p2 - 4);

            string sql = "SELECT * FROM user WHERE NAME=\"";
            sql += username;
            sql += "\";";
            cout << "sql语句:" << sql << endl;
            auto search_res = mysql_query(con, sql.c_str());
            auto result = mysql_store_result(con);
            int row;
            if(result){
                row = mysql_num_rows(result);
            }
            if(search_res == 0 && row != 0){
                cout << "查询成功" << endl;
                auto info = mysql_fetch_row(result);//获取一行的信息
                cout << "查询到用户名:" << info[0] << " 密码:" << info[1] << endl;
                if(info[1] == passwd){
                    cout << "登录密码正确\n";
                    string str1 = "ok";
                    is_login = true;
                    login_name = username;
                    pthread_spin_lock(&name_mutex); //上锁
                    name_sock_map[username] = con_fd;//记录下名字和套接字描述符的对应关系
                    pthread_spin_unlock(&name_mutex); //解锁
                    
                    send(con_fd, str1.c_str(), str1.length() + 1, 0);
                }else{
                    cout << "密码错误!" << endl;
                    char str1[100] = "wrong";
                    send(con_fd, str1, strlen(str1), 0);
                }
            }else{
                cout << "查询失败\n";
                char str1[100] = "wrong";
                send(con_fd, str1, strlen(str1), 0);
            }
        }
        //处理注册
        else if(str.find("name:") != str.npos){
            int p1 = str.find("name:"), p2 = str.find("pass:");
            username = str.substr(p1+5, p2-5);
            passwd = str.substr(p2+5,str.length()-p2-4);
            string sql = "INSERT INTO user VALUES (\"";
            sql += username;
            sql += "\",\"";
            sql += passwd;
            sql += "\");";
            cout << endl << "sql语句:" << sql <<endl;
            mysql_query(con, sql.c_str());
        }
        //设定目标的套接字描述符
        else if(str.find("target:") != str.npos){
            int pos1 = str.find("from:");
            //获取源用户和目标用户的名称
            string target = str.substr(7, pos1-7);
            string from = str.substr(pos1 + 5);
            target_name = target; //将目标用户名赋给全局变量
            
            //如果在name_sock_map中没有找到target对应的套接字描述符，则说明对方还未登录
            if(name_sock_map.find(target) == name_sock_map.end())
                cout << "源用户为" << from << ",目标用户" << target_name << "仍未登陆，无法发起私聊" << endl;
            else{
                //利用自旋锁锁定，减少上下文切换的开销
                pthread_spin_lock(&from_mutex);
                from_to_map[from] = target; //将源用户和目标用户的名称一一对应并存在map中
                pthread_spin_unlock(&from_mutex);
                login_name = from;
                cout << "源用户" << login_name << "向目标用户" << target_name << "发起的私聊即将建立";
                cout << ",目标用户的套接字描述符为" << name_sock_map[target] << endl;
                target_con_fd = name_sock_map[target]; //取出目标用户的套接字赋给target_con_fd以便接下来的通信
            }      
        }
        //接收到消息，转发消息
        else if(str.find("content:") != str.npos){
            //根据两个map找出当前用户和目标用户
            for(auto i : name_sock_map){
                if(i.second == con_fd){
                    login_name = i.first;
                    target_name = from_to_map[i.first];
                    target_con_fd = name_sock_map[target_name];
                    break;
                }       
            }
            if(target_con_fd == -1){
                cout << "找不到目标用户" << target_name << "的套接字，将尝试重新寻找目标用户的套接字\n";
                if(name_sock_map.find(target_name) != name_sock_map.end()){
                    target_con_fd = name_sock_map[target_name];
                    cout << "重新查找目标用户套接字成功\n";
                }
                else{
                    cout << "查找仍然失败，转发失败！\n";
                    // continue;
                }
            }
           
            string recv_str(str);
            string send_str = recv_str.substr(8);
            cout << "用户" << login_name << "向" << target_name << "发送:" << send_str << endl;
            send_str = "[" + login_name + "]:" + send_str;
            send(target_con_fd, send_str.c_str(), send_str.length(), 0);
        }
        //绑定群聊号
        else if(str.find("group:") != str.npos){
            string recv_str(str);
            string num_str=recv_str.substr(6);
            group_num = stoi(num_str);
            //找出当前用户
            for(auto i : name_sock_map)
                if(i.second == con_fd){
                    login_name = i.first;
                    break;
                }
            cout << "用户" << login_name << "绑定群聊号为：" << num_str << endl;
            pthread_spin_lock(&group_mutex);//上锁
            //group_map<int, set<int>>:保存了一个群号中所有的套接字描述符
            group_map[group_num].insert(con_fd);
            pthread_spin_unlock(&group_mutex);//解锁
        }
        //广播群聊信息
        else if(str.find("gr_message:") != str.npos){
            //找出当前用户
            for(auto i : name_sock_map)
                if(i.second == con_fd){
                    login_name = i.first;
                    break;
                }
            //找出群号
            for(auto i : group_map)
                //如果当前用户的套接字描述符在group_map中，则将群号获取到
                if(i.second.find(con_fd) != i.second.end()){
                    group_num = i.first;
                    break;
                }
            string send_str(str);
            send_str = send_str.substr(11);
            send_str = "["+login_name+"]:" + send_str;
            cout << "群聊信息：" << send_str << endl;
            //遍历该群中所有的套接字描述符，挨个发消息
            for(auto i : group_map[group_num]){
                if(i != con_fd)
                    send(i, send_str.c_str(), send_str.length(), 0);
            }       
        }  
        cout<<"---------------------"<<endl;

        //线程工作完毕后重新注册事件
        //如果不重新注册事件，则服务器无法接收消息
        epoll_event event;
        event.data.fd = con_fd;
        event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
        epoll_ctl(epollfd, EPOLL_CTL_MOD, con_fd, &event);
        
        mysql_close(con);
    }   
}