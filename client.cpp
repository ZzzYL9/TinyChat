#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <unistd.h>
#include <atomic>
#include <stdlib.h>
#include <fcntl.h>
// #include <sys/shm.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <thread>
#include <chrono>
#include "HandleClient.h"
using namespace std;
using namespace std::chrono;

// 登录请求信息
struct LoginRequest {
    string username;
    string password;
};

// 读取账号信息
vector<LoginRequest> readAccountInfo(const string& filename) {
    vector<LoginRequest> accountInfo;

    ifstream file(filename);
    if (!file) {
        cout << "Failed to open account file: " << filename << endl;
        return accountInfo;
    }

    string line;
    while (getline(file, line)) {
        istringstream iss(line);
        string username, password;
        if (!(iss >> username >> password)) {
            cout << "Invalid account info: " << line << endl;
            continue;
        }
        accountInfo.push_back({username, password});
    }

    return accountInfo;
}

// 连接并发送登录请求
void connectAndSendLoginRequest(const string& serverIP, int serverPort, const LoginRequest& loginRequest, atomic<int>& successCount) {
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        cout << "Failed to create socket" << endl;
        return;
    }

    struct sockaddr_in servAddr{};
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = inet_addr(serverIP.c_str());
    servAddr.sin_port = htons(serverPort);

    if (connect(sock, (struct sockaddr *)&servAddr, sizeof(servAddr)) == -1) {
        cout << "Connection failed for username: " << loginRequest.username << endl;
        close(sock);
        return;
    }

    string loginStr = "login " + loginRequest.username + " pass: " + loginRequest.password;
    if (send(sock, loginStr.c_str(), loginStr.length(), 0) < 0) {
        cout << "Failed to send login request for username: " << loginRequest.username << endl;
        close(sock);
        return;
    }

    // 在这里可以接收服务器的响应并进行处理

    close(sock);
    successCount++;
}


int main(){

    const string serverIP = "127.0.0.1";
    const int serverPort = 8023;
    const string accountFile = "./account.txt";

    // 读取账号信息
    vector<LoginRequest> accountInfo = readAccountInfo(accountFile);
    if (accountInfo.empty()) {
        cout << "No valid account info found" << endl;
        return 0;
    }

    atomic<int> successCount(0); // 记录成功登录的次数

    high_resolution_clock::time_point startTime = high_resolution_clock::now();

    vector<thread> threads;
    for (const auto& account : accountInfo) {
        threads.emplace_back([serverIP, serverPort, account, &successCount]() {
            connectAndSendLoginRequest(serverIP, serverPort, account, successCount);
        });
    }

    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }

    high_resolution_clock::time_point endTime = high_resolution_clock::now();
    double elapsedTime = duration_cast<duration<double>>(endTime - startTime).count();
    int totalRequests = accountInfo.size();
    int successfulRequests = successCount;

    cout << "Performance Summary:" << endl;
    cout << "Total Requests: " << totalRequests << endl;
    cout << "Successful Requests: " << successfulRequests << endl;
    cout << "QPS: " << successfulRequests / elapsedTime << endl;

    return 0;


    // int sock;
    // struct sockaddr_in serv_addr;
    // pthread_t send_thread, rcev_thread; //创建两个线程分别用来读写
    // //PF_INET 指定了套接字使用的网络协议是 IPv4
    // //SOCK_STREAM 指定了套接字使用的传输层协议是 TCP
    // sock = socket(PF_INET, SOCK_STREAM, 0);

    // memset(&serv_addr, 0, sizeof(serv_addr));
    // serv_addr.sin_family = AF_INET; //将服务地址设置为 IPv4 地址族
    // serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    // serv_addr.sin_port = htons(8023);

    // if(connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1){
    //     cout << "connect()函数错误！";
    //     perror("connect");
    // }

    // int choice = 0;
    // string username, pwd, pwd_check; //用户名，密码，确认密码
    // bool is_login = false; //判断是否登陆成功
    // string login_username ; //记录登录的用户名

    // //未登录时
    // if(!is_login){
    //     cout << " ------------------\n";
    //     cout << "|                  |\n";
    //     cout << "| 请输入你要的选项:|\n";
    //     cout << "|    0:退出        |\n";
    //     cout << "|    1:登录        |\n";
    //     cout << "|    2:注册        |\n";
    //     cout << "|                  |\n";
    //     cout << " ------------------ \n\n";
    // }

    // //开始处理各种事务
    // while(1){
    //     if(is_login)
    //         break;
    //     cin >> choice;
    //     if(choice == 0)
    //         break;
    //     //登录
    //     else if(choice == 1 && !is_login){
    //         while(1){
    //             cout << "用户名:";
    //             cin >> username;
    //             cout << "密码:";
    //             cin >> pwd;
    //             string str = "login" + username;
    //             str += "pass:";
    //             str += pwd;
    //             send(sock, str.c_str(), str.length(), 0);//发送登录信息
    //             char buffer[1000];
    //             memset(buffer, 0, sizeof(buffer));
    //             recv(sock, buffer, sizeof(buffer), 0);//接收响应
    //             string recv_str(buffer);
    //             if(recv_str.substr(0,2) == "ok"){
    //                 is_login = true;
    //                 login_username = username;

    //                 cout << "登录成功\n\n";
    //                 break;
    //             }
    //             else
    //                 cout << "密码或用户名错误！\n\n";
    //         }       
    //     }
    //     //注册
    //     else if(choice == 2){
    //         cout << "注册的用户名:";
    //         cin >> username;
    //         while(1){
    //             cout << "密码:";
    //             cin >> pwd;
    //             cout << "确认密码:";
    //             cin >> pwd_check;
    //             if(pwd == pwd_check)
    //                 break;
    //             else
    //                 cout << "两次密码不一致!\n\n";
    //         }   
    //         username = "name:" + username;
    //         pwd = "pass:" + pwd;
    //         string str = username + pwd;
    //         send(sock, str.c_str(), str.length(), 0);
    //         cout << "注册成功！\n";
    //         cout << "\n继续输入你要的选项:";     
    //     }
    //     if(is_login)
    //         break;
    // }
    // // ssize_t a = send(sock, s.c_str(), s.length(), 0);
    // // if(a == -1){
    // //     if(errno == EINTR){
    // //         cout << "发送被中断" << endl;
    // //     }else if(errno == EAGAIN || errno == EWOULDBLOCK){
    // //         cout << "发送区缓冲区已满" << endl;
    // //     }else{
    // //         perror("send");
    // //     }
    // // }else{
    // //     cout << "成功" << endl;
    // // }
    // // sleep(5);
    // //登陆成功
    // while(is_login && 1){
    //     if(is_login){
    //         system("clear");
    //         cout << "        欢迎回来,"<< login_username << endl;
    //         cout << " -------------------------------------------\n";
    //         cout << "|                                           |\n";
    //         cout << "|          请选择你要的选项：               |\n";
    //         cout << "|              0:退出                       |\n";
    //         cout << "|              1:发起单独聊天               |\n";
    //         cout << "|              2:发起群聊                   |\n";
    //         cout << "|                                           |\n";
    //         cout << " ------------------------------------------- \n\n";
    //     }
    //     cin >> choice;
    //     pthread_t send_t, recv_t;//线程ID
    //     void *thread_return; //获取线程的返回值
    //     if(choice == 0)
    //         break;
    //     if(choice == 1){
    //         cout << "请输入对方的用户名:";
    //         string target_name, content;
    //         cin >> target_name;
    //         string sendstr("target:" + target_name + "from:" + login_username);//标识目标用户+源用户
    //         ssize_t a = send(sock, sendstr.c_str(), sendstr.length(), 0);//先向服务器发送目标用户、源用户
            
    //         if(a == -1){
    //             if(errno == EINTR){
    //                 cout << "发送被中断" << endl;
    //             }else if(errno == EAGAIN || errno == EWOULDBLOCK){
    //                 cout << "发送区缓冲区已满" << endl;
    //             }else{
    //                 perror("send");
    //             }
    //         }


    //         cout << "请输入你想说的话(输入exit退出)：\n";
    //         auto send_thread = pthread_create(&send_t, NULL, handle_send, (void *)&sock);//创建发送线程
    //         auto recv_thread = pthread_create(&recv_t, NULL, handle_recv, (void *)&sock);//创建接收线程
    //         //确保主线程在发送线程执行完毕之前不会继续执行后续代码
    //         pthread_join(send_t, &thread_return);
    //         pthread_cancel(recv_t); //向接收线程发送取消请求
    //     }    
    //     if(choice == 2){
    //         cout << "请输入群号:";
    //         int num;
    //         cin >> num;
    //         string sendstr("group:" + to_string(num));
    //         send(sock,sendstr.c_str(), sendstr.length(), 0); //先把群号发给服务器绑定
    //         cout << "请输入你想说的话(输入exit退出)：\n";
    //         int sock1 = -sock; //用来区别私聊还是群发消息
    //         auto send_thread = pthread_create(&send_t, NULL, handle_send, (void *)&sock1);//创建发送线程
    //         auto recv_thread = pthread_create(&recv_t, NULL, handle_recv, (void *)&sock);//创建接收线程
    //         pthread_join(send_t, &thread_return);
    //         pthread_cancel(recv_t);
    //     }
    // } 
    // close(sock);
}

