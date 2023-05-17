#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>

//字符串處理函數
void deal_argv(char input[128], char new_argv[][256]){
    
    char tmp[256];
    memset(tmp, 0, sizeof(tmp));
    int j = 0;  //記錄參數個數
    int k = 0;  //記錄tmp

    for(int i=0;i<strlen(input);i++){
        //printf("getin\n");
        if(input[i]!=' '&&input[i]!='\n'){
            //printf("%c\n", input[i]);
            tmp[k] = input[i];
            k++;
        }else{
            memcpy(new_argv[j], tmp, sizeof(char)*strlen(tmp));
            //printf("%s\n", new_argv[j]);
            memset(tmp, 0, sizeof(tmp));
            k = 0;
            j++;
        }
        
    }

    return 0;
}

//統一的報頭格式
struct post{   
    char m_protocol[6]; 
    char m_type;                     //1 byte
    char m_status;                   //1 byte
    uint32_t m_length;                    //大端法
} __attribute__ ((packed));

//const char pro[6]="\xe3myftp";

//格式化填充報文頭部並發送
void post_head(int client_sock, char ty, char sta, int len){
    struct post client_post;
    char pro[6]="\xe3myftp";
    memcpy(client_post.m_protocol, pro, sizeof(pro)+1);
    client_post.m_type = ty;
    //printf("protocol: %s, size: %d\n", client_post.m_protocol, sizeof(client_post.m_protocol));
    //printf("type: %d\n", client_post.m_type);
    client_post.m_status = sta;
    //printf("status: %c\n", client_post.m_status);
    client_post.m_length = htonl(len);
    //printf("length: %d\n", ntohl(client_post.m_length));
    //printf("sizeof: %d\n", sizeof(client_post));

    //發送報文頭部
    send(client_sock, &client_post, 12, 0);
    //printf("send post-head success\n");
    return;
}

/*
open <IP> <port>: 建立一个到<IP>:<port>的连接
auth <username> <password>: 向对侧进行身份验证
ls: 获取对方当前运行目录下的文件列表
get <filename>: 将Server运行目录中的<filename>文件存放到Client运行目录的<filename>中
put <filename>: 将Client运行目录中的<filename>文件存放当Server运行目录的<filename>中
quit: 如有连接则先断开，后关闭Client
*/

//發送建立鏈接的報文頭部
void Open_a_Connection(int client_sock){
    
    post_head(client_sock, 0xA1, 0, 12 );

    char buffer[13];
    memset(buffer, 0, sizeof(buffer));
    recv(client_sock, buffer, 12, 0);

    if(buffer[7] != 1){
        printf("server is off\n");
    memset(buffer, 0, sizeof(buffer));
        printf("請重新調用open打開端口\n");
    }
    else{
        printf("Connection accepted\n");
        printf("接下來請調用auth進行驗證操作\n");
    }
    return 0;
 }

//再度打開端口
int Open_again(char *ip, char *port){

    int client_sock;
    client_sock = socket(AF_INET, SOCK_STREAM, 0); //創建客戶套接字
    struct sockaddr_in addr;
    addr.sin_port = htons(atoi(port));
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &addr.sin_addr); // 表示我们要连接到服务器的127.0.0.1:23233
    connect(client_sock, (struct sockaddr*)&addr, sizeof(addr));
    printf("已建立連接，請發送報文\n");
    
    //發送報文頭部
    Open_a_Connection(client_sock);
    return client_sock;
}

 //驗證用戶名密碼       
int Authentication(int client_sock, char name[], char password[]){
    
    size_t len = 12+1+strlen(password)+strlen(name)+1;
    post_head(client_sock, 0xA3, 0, len);

    //拼接用戶名和密碼，發送payload
    size_t msg_len = strlen(password)+strlen(name)+1+1;
    char name_pass[256];
    memset(name_pass, 0, sizeof(name_pass));

    size_t name_tmp = sizeof(char)*strlen(name);
    memcpy(name_pass, name, name_tmp);
    name_pass[strlen(name)]=' ';
    size_t pass_tmp = sizeof(char)*strlen(password);
    memcpy(name_pass+strlen(name)+1, password, pass_tmp);
    name_pass[msg_len-1] = '\0';

    printf("payload: %s, size is : %d\n", name_pass, msg_len);
    
    send(client_sock, name_pass, msg_len, 0);
    printf("payload is sent\n");

    char buffer[256];
    memset(buffer, 0, sizeof(buffer));
    recv(client_sock, buffer, 256, 0);
    printf("buffer: %s\n", buffer);
    if(buffer[7] != 1){
        printf("Authentication rejected\n");
        printf("請重新調用open打開端口\n");
        close(client_sock);
        return 0;
    }
    else{
        printf("Authentication success\n");
        printf("可以進行main函數操作了\n");
    }

    return 1;
}

//列出server的文件目錄
void List_Files(int client_sock){

    printf("----- file list start -----\n");
    int len = 12;
    post_head(client_sock, 0xA5, 0, len);

    //接收server報文頭部
    char ls_buff[13];
    memset(ls_buff, 0, sizeof(ls_buff));
    recv(client_sock, ls_buff, 12, 0);

    //判斷接收到的包頭總長度
    char ls_tmp[4];
    uint32_t sum = 0;
    unsigned int length;
    memset(ls_tmp, 0, sizeof(ls_tmp));
    ls_tmp[0]=ls_buff[8];
    ls_tmp[1]=ls_buff[9];
    ls_tmp[2]=ls_buff[10];
    ls_tmp[3]=ls_buff[11];
    length = *(unsigned int*)ls_tmp;
    sum = ntohl(length);
    printf("轉化後的包頭長度 : %d\n", sum);

    char list_file[2048];
    memset(list_file, 0, sizeof(list_file));
    size_t ret = 0;
    size_t b = 0;
    uint32_t list_len = sum - 12;  
    while (1) {
        b = recv(client_sock, list_file + ret, list_len - ret, 0);
        printf("this round recieved: %d\n", b);

        ret += b;
        if (ret == list_len) {
            printf("all recieved, socket Closed\n");
            break;
        } 
        // 成功将b个byte塞进了缓冲区
        printf("this round ret is: %d\n", ret);
    }

    //輸出文件目錄
    printf("begin to list files\n");
    fputs(list_file, stdout);
    
    printf("----- file list end -----\n");
    
    return 0;
}

//下載文件
void Download_Files(int client_sock, char name[256]){

    int len = 12+1+strlen(name);
    post_head(client_sock, 0xA7, 0, len);

    char n_name[256];
    memset(n_name, 0, sizeof(n_name));
    memcpy(n_name, name, strlen(name)*sizeof(char));
    n_name[strlen(name)] = '\0';
    printf("期望得到文件名：%s, 實際得到的文件名： %s\n", name, n_name);
    send(client_sock, n_name, strlen(name)+1, 0);

    //收到第一個報頭
    char buffer_rcv[13];
    memset(buffer_rcv, 0, sizeof(buffer_rcv));
    recv(client_sock, buffer_rcv, 12, 0);
    
    //判斷能否成功下載
    if(buffer_rcv[7] != 1){
        printf("下載失敗\n");
        return 0;
    }
    else{

        printf("已找到該文件\n");
        printf("開始下載中-----\n");
        char get_file[2048];
        memset(get_file, 0, sizeof(get_file));
        size_t ret = 0;
        size_t b = 0;
        uint32_t get_len = 2048;
        uint32_t sum = 0;
        char get_tmp[4];
        unsigned int length;
        while (1) {
            b = recv(client_sock, get_file + ret, get_len - ret, 0);
            printf("this round recieved: %d\n", b);

            //判斷接受到的文件總長度
            memset(get_tmp, 0, sizeof(get_tmp));
            get_tmp[0]=get_file[8];
            get_tmp[1]=get_file[9];
            get_tmp[2]=get_file[10];
            get_tmp[3]=get_file[11];
            length = *(unsigned int*)get_tmp;
            sum = ntohl(length);
            printf("希望得到的文件大小 : %d\n", sum-12);

            ret += b;
            if (ret == sum) {
                printf("all recieved, socket Closed\n");
                break;
            } 
            // 成功将b个byte塞进了缓冲区
            printf("this round ret is: %d\n", ret);
        }

        get_file[strlen(get_file)] = '\0'; //增加休止符
        printf("目的文件內容爲： %s\n", get_file+12);

        FILE *fd1;
        //打开文件
        fd1 = fopen(name, "wb");  
        if (fd1 == NULL) {
            printf("打开或者新建文件失败\n");
            return 0;
        }
        printf("正在寫入中---\n");

        int i = 12;
        while (get_file[i]!='\0') {
            fputc(get_file[i], fd1);  
            i++;  
        }
        
        fclose(fd1);   
        printf("已下載該文件到當前目錄\n");

    }
    return 0;
}

//put 传送给远方一个文件
//上傳文件
void Upload_Files(int client_sock, char name[256]){

    FILE *fd2;
    fd2 = fopen(name, "rb");
    if (fd2) { 
        //成功打开
        printf("成功打開了 %s 文件\n", name);
        int len = 12+1+strlen(name);
        post_head(client_sock, 0xA9, 0, len);

        char n_name[256];
        memset(n_name, 0, sizeof(n_name));
        memcpy(n_name, name, sizeof(char)*strlen(name));
        n_name[strlen(name)] = '\0';
        //發送文件名
        send(client_sock, n_name, strlen(name)+1, 0);
        printf("文件名已發送\n");
        printf("文件名是 :%s\n", n_name);
        
        char buffer_rcv[13];
        memset(buffer_rcv, 0, sizeof(buffer_rcv));
        recv(client_sock, buffer_rcv, 12, 0);
        
        printf("正在传输文件…\n");
        // 这是一个存储文件(夹)信息的结构体，其中有文件大小和创建时间、访问时间、修改时间等
        struct stat statbuf;
        // 提供文件名字符串，获得文件属性结构体
        stat(name, &statbuf);
        // 获取文件大小
        size_t filesize = statbuf.st_size;
        post_head(client_sock, 0xFF, 0, 12+filesize);

        char sbuff[2048];  //發送緩衝區
        memset(sbuff, 0, sizeof(sbuff));

        while (1) { //从文件中循环读取数据并发送
            int read_sum = fread(sbuff, 1, sizeof(sbuff), fd2); 
            printf("this time read :%d\n", read_sum);
            //fread从file文件读取sizeof(sbuff)长度的数据到sbuff，返回成功读取的数据个数

            if (!send(client_sock, sbuff, strlen(sbuff), 0)) {
                printf("与客户端的连接中断\n");
                return 0;
            }
            if (read_sum < sizeof(sbuff)) {
                break;
            }
            memset(sbuff, 0, sizeof(sbuff));
        }
        printf("传输完成\n");

        fclose(fd2);
    } else {
        printf("无法打开文件\n");
    }
    
    return 0;
}

//關閉連接
void Close_Connection(int client_sock){

    
    int len = 12;
    post_head(client_sock, 0xAB, 0, len);

    char buffer_rcv[13];
    memset(buffer_rcv, 0, sizeof(buffer_rcv));
    recv(client_sock, buffer_rcv, 12, 0);

    close(client_sock);
    printf("已關閉\n");
    return 0;
}

//整體報錯接口，判斷main函數所在的狀態機前置是否完成
int err(int sock, int authed){
    if(!sock){
        printf("請先使用open打開接口\n");
        return 1;
    }
    if(!authed){
        printf("請先使用auth進行驗證\n");
        return 1;
    }

    return 0;
}


int main(int argc, char ** argv) {

    //每次判斷狀態機
    int client_sock = 0;
    int authed = 0;

    //鏈接後循環等待函數輸入
    while(1){
        printf("Client > ");
        char buffer[128];
        memset(buffer, 0, sizeof(buffer));
        fgets(buffer, 128, stdin);  //從標準輸入讀入數據
        char new_argv[3][256];
        memset(new_argv[0], 0, sizeof(new_argv[0]));
        memset(new_argv[1], 0, sizeof(new_argv[1]));
        memset(new_argv[2], 0, sizeof(new_argv[2]));
        deal_argv(buffer, new_argv);  //將最多三個參數同時讀入
        //printf("%s\n", new_argv[0]);
        //執行對應操作
        
        if(!strcmp(new_argv[0], "open")){
            printf("start open\n");
            client_sock = Open_again(new_argv[1], new_argv[2]); 
        }
        else if(!strcmp(new_argv[0], "auth")){
            if(!client_sock){
                    printf("請先使用open打開接口\n");
                    continue;
                }   
            printf("start auth\n");
            authed = Authentication(client_sock, new_argv[1], new_argv[2]); 
            if(!authed) client_sock = 0;
        }   
        else if(!strcmp(new_argv[0], "ls")){
            if(!err(client_sock, authed)) {
                printf("start ls\n");
                List_Files(client_sock);
            }  
                else continue;
        } 
        else if(!strcmp(new_argv[0], "get")){
             if(!err(client_sock, authed)) {
                printf("start get\n");
                Download_Files(client_sock, new_argv[1]);
             }  
                else continue;
        }   
        else if(!strcmp(new_argv[0], "put")){
            if(!err(client_sock, authed))  {
                printf("start put\n");
                Upload_Files(client_sock, new_argv[1]);
            } 
                else continue;
        }  
        else if(!strcmp(new_argv[0], "quit")){
            if(!err(client_sock, authed)){
                printf("start close\n");
                Close_Connection(client_sock);
                client_sock = 0;
                authed = 0;
            }   
                else continue;
        }
        else{
            printf("Wrong Request!\n");
            continue;
        }
    }

    return 0;
}