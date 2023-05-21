#include"HandleClient.h"
using namespace std;

//pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void *handle_recv(void *arg){
    int sock = *(int *)arg;
    while(1){
        char recv_buffer[1000];
        memset(recv_buffer, 0, sizeof(recv_buffer));
        int len = recv(sock, recv_buffer, sizeof(recv_buffer), 0);
        if(len == 0)
            continue;
        if(len == -1)
            break;
        string str(recv_buffer);
        cout << str << endl;
    }
}

void *handle_send(void *arg){
    int sock = *(int *)arg;
    while(1){
        string str;
        cin >> str;
        if(str == "exit")
            break;
        if(sock > 0){
            str = "content:" + str;
            send(sock, str.c_str(), str.length(), 0);
        }
        else if(sock < 0){
            str = "gr_message:"+str;
            send(-sock, str.c_str(), str.length(), 0);
        }   
    }   
}




