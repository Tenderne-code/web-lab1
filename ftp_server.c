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

//統一的報頭格式
struct post{   
    char m_protocol[6]; 
    char m_type;                     //1 byte
    char m_status;                   //1 byte
    uint32_t m_length;                    //大端法
} __attribute__ ((packed));

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
 1.权限控制: 用户需要登录这里简化为用户名为user密码为123123
 2.获取文件列表: 这里文件列表由指令ls生成，可以使用popen或者pipe+fork+execv的手段获取其他进程的输出结果
 3.下载文件
 4.上传文件
 */

//發送建立鏈接的報文頭部
int Open_a_Connection(int client_sock){

    
    post_head(client_sock, 0xA2, 1, 12 );
    printf("Connection accepted\n");
    printf("接下來請調用auth進行驗證操作\n");
    return 1;
 }
    
 //驗證用戶名密碼       
int Authentication(int client_sock, char auth_buff[256]){
    
    size_t len = strlen(auth_buff)+1;
    printf("payload長度是: %d\n", len);
    printf("開始驗證\n");
    
    //分別驗證用戶名和密碼
    char name[128];
    char pass[128];
    char tmp[128];
    memset(name, 0, sizeof(name));
    memset(pass, 0, sizeof(pass));
    memset(tmp, 0, sizeof(tmp));

    int j = 0;
    for(int i=0;i<len;i++){
        if(auth_buff[i]==' '){
            memcpy(name, tmp, strlen(tmp));
            j = 0;
            memset(tmp, 0, sizeof(tmp));
        }
        else{
            tmp[j] = auth_buff[i];
            j++;
        }
        
    }
    memcpy(pass, tmp, strlen(tmp));
    printf("name: %s, pass is : %s\n", name, pass);
    if(!strcmp(name, "user")&&!strcmp(pass, "123123")){
        post_head(client_sock, 0xA4, 1, 12);
        printf("驗證成功！可以進行下一步了\n");
        return 1;
    }
    else{
        post_head(client_sock, 0xA4, 0, 12);
        printf("驗證失敗! 請重新open端口\n");
    }
    return 0;
}

//列出server的文件目錄
void List_Files(int client_sock){

    printf("----- file list start -----\n");
    FILE *fd = popen("ls", "r");
    
    char list_file[2048];
    memset(list_file, 0, sizeof(list_file));
    int read_sum = fread(list_file, 1, sizeof(list_file), fd); 
    list_file[read_sum] = '\0';
    int len = 12+1+read_sum;
    post_head(client_sock, 0xA6, 0, len);
    printf("將發送的payload總長度: %d\n", len-12);
    //sleep(10000000);
    
    size_t ret = 0;
    size_t b = 0;
    while (1) {
        b = send(client_sock, list_file + ret, read_sum+1 - ret, 0);
        printf("本輪發送了： %d\n", b);

        ret += b;
        if (ret == len-12) {
            printf("已經全部上傳\n");
            break;
        } 
        //成功将b个byte塞进了缓冲区
        printf("this round ret is: %d\n", ret);
    }
    printf("传输完成\n");

    pclose(fd);
    printf("----- file list end -----\n");
    
    return 0;
}

//下載文件
void Download_Files(int client_sock, char name[256]){

    printf("已找到該文件\n");
    printf("開始接收中-----\n");

    post_head(client_sock, 0xAA, 0, 12);
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
            printf("已經全部上傳\n");
            break;
        } 
        //成功将b个byte塞进了缓冲区
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
        post_head(client_sock, 0xA8, 1, 12);
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
        post_head(client_sock, 0xA8, 0, 12);
    }
    
    return 0;
}

//關閉連接
void Close_Connection(int client_sock){

    int len = 12;
    post_head(client_sock, 0xAC, 0, len);

    close(client_sock);
    printf("已關閉服務器\n");
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

//處理payload長度
int deal_payload(char headpost[12]){

    char tmp[4];
    uint32_t sum = 0;
    unsigned int length = 0;
    memset(tmp, 0, sizeof(tmp));
    tmp[0]=headpost[8];
    tmp[1]=headpost[9];
    tmp[2]=headpost[10];
    tmp[3]=headpost[11];
    length = *(unsigned int*)tmp;
    sum = ntohl(length);
    printf("報文總長度 : %d\n", sum);
    return sum;
}

int main(int argc, char ** argv) {

    //每次判斷狀態機
    int sock;
    sock = socket(AF_INET, SOCK_STREAM, 0); // 申请一个TCP的socket
    struct sockaddr_in addr; // 描述监听的地址
    addr.sin_port = htons(atoi(argv[2])); // 端口监听 htons是host to network (short)的简称，表示进行大小端表示法转换，网络中一般使用大端法
    addr.sin_family = AF_INET; // 表示使用AF_INET地址族
    inet_pton(AF_INET, argv[1], &addr.sin_addr); // 监听地址，将字符串表示转化为二进制表示
    bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    listen(sock, 128);
    int client_sock = 0;
    printf("Server is ready\n");
    //printf("已與指定端口建立連接,下面發送回覆報文\n");
    int authed = 0;
    //針對該客戶端的循環
    while(1){
        printf("請先使用open建立連接\n");
        client_sock = accept(sock, NULL, NULL);
        //open和auth的大循環
        while(1){
        printf("正在進行open操作\n");
        char buffer[13];
        memset(buffer, 0, sizeof(buffer));
        recv(client_sock, buffer, 12, 0);
        printf("buffer: %s\n", buffer);
        if(buffer[6]==(char)0xA1){
            Open_a_Connection(client_sock);
            break;
        }
        }
       
        //鏈接後循環等待函數輸入
        //針對同一客戶端不同要求的循環
        while(1){
            printf("Server > ");
            char buffer[13];
            memset(buffer, 0, sizeof(buffer));
            recv(client_sock, buffer, 12, 0);
            //得到payload長度
            int pay_len = deal_payload(buffer);
            char request_type = buffer[6];
            //printf("buffer[6] is : %d\n", (unsigned int)request_type);

            if(request_type == (char)0xA3){
                printf("請求驗證用戶名密碼中---\n");
                char auth_buff[256];
                memset(auth_buff, 0, sizeof(auth_buff));
                //讀入剩下的payload
                recv(client_sock, auth_buff, pay_len-12, 0);
                printf("收到的payload: %s\n", auth_buff);

                authed = Authentication(client_sock, auth_buff);
                if(!authed) {
                    printf("驗證失敗,請重新進行open操作\n");
                    close(client_sock);
                    printf("端口已關閉\n");
                    client_sock = 0;
                    break;
                }
            }
            else if(request_type == (char)0xA7){
                printf("進入get了\n");
                char get_buff[256];
                memset(get_buff, 0, sizeof(get_buff));
                //讀入剩下的payload
                recv(client_sock, get_buff, pay_len-12, 0);
                printf("收到的payload: %s\n", get_buff);
                if(!err(client_sock, authed)) {
                    printf("start get\n");
                    
                    Upload_Files(client_sock, get_buff);
                }  
                else break;
            }   
            else if(request_type == (char)0xA9){
                printf("進入put了\n");
                char put_buff[256];
                memset(put_buff, 0, sizeof(put_buff));
                //讀入剩下的payload
                recv(client_sock, put_buff, pay_len-12, 0);
                printf("收到的payload: %s\n", put_buff);
                if(!err(client_sock, authed)) {
                    printf("start put\n");
                    Download_Files(client_sock, put_buff);
                }  
                    else break;
            } 
            else if(request_type == (char)0xAB){
                if(!err(client_sock, authed)) {
                    printf("quit start\n");
                    Close_Connection(client_sock);
                    client_sock = 0;
                    authed = 0;
                    break;
                }  
                    else break;
            }   
            else if(request_type == (char)0xA5){
                if(!err(client_sock, authed))  {
                    printf("start list\n");
                    List_Files(client_sock);
                } 
                    else break;
            } 
            else{
                printf("Wrong Request!\n");
                continue;
            }
        }

    }
    
    return 0;
}