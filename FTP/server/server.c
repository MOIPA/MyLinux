#include<sys/socket.h>
#include<dirent.h>
#include<stdlib.h>
#include<sys/types.h>
#include<arpa/inet.h>
#include<pthread.h>
#include<string.h>
#include<sys/stat.h>
#include<netinet/in.h>
#include<stdio.h>
#include<unistd.h>

#define USER_ACCOUNTS 1  //Max users
#define MAX_THREAD  100	//the number of clients that server can handle at one time
#define DEFAULT_IP "127.0.0.1"
#define DEFAULT_PORT 8888
#define DEFAULT_DIR "/home/tr/server"
#define MKDIR	1
#define RMDIR	2
#define PUT		3
#define GET		4
#define PWD		5
#define BINARY	6
#define LS		7
#define CD		8
#define QUIT	9
#define ASCALL 10
#define UNKNOWN 11
#define FINISH 12

typedef struct {
        char thread_name[20];
        pthread_t thread;
}ClientThreads;

typedef struct {
        char account_name[30];
        char account_password[30];
        int authority;//user power to judge what user can do
}ACCOUNT;

typedef struct {
        int client_sockfd;
        int authority;
}ClientInfo;

enum Authority
{
        rootLevel,commonLevel,visitorLevel
};
struct sockaddr_in *initServerSock(int *server_sockfd,char *ip,int port);//init server socket do bind and listen
void initThreadLists();//init thread pool
ACCOUNT* CreateAccounts();//create users root user who can do everything 
ACCOUNT Login(int client_sock);// do login return user login account
int SplitByTag(char tag,const char *complete_input,char* out_put_command,char *parameter); //split into two string by tag
void *ServerThread(void *arg);	//server thread function
void *ClientThread(void *arg);	//client thread function
int GetUnUsedThread();
int GetUserAuthority(char *account_name,char *account_password,ACCOUNT* user_lists);
int GetCommandKind(char *cmd);
void JudgeAndExecute(int client_sock, int user_authority, int thread_id);

void ExecuteGet(int client_sock,char *file_name);
void ExecutePut(int client_sockfd,char *file);
void ExecuteMkdir(int client_sockfd,char *parameter);
void ExecuteRmDir(int client_sockfd,char *parameter);
void ExecuteLs(int client_sockfd,char *path);
void ExecuteCd(int client_sockfd,char *parameter);


//shared resources 
int total_accounts=0,current_accounts=0,binary=1;
ClientThreads thread_lists[MAX_THREAD];


int main(int arg,char* argv[]){

        //define socket
        int server_sock,client_sock;
        socklen_t len_of_sockaddr_in = sizeof(struct sockaddr_in);
        struct sockaddr_in *server_sockaddr;
        //define thread
        pthread_t client_thread;
        pthread_t server_thread;
        //define account
        ACCOUNT login_user;
        ACCOUNT *accounts;
        int user_authority;

        ClientInfo client_info; 
        char buf[1024];

        initThreadLists();
        accounts = CreateAccounts();
        //init server address and port
        if(arg!=3){
                printf("correct form is:ip port use default config!\n");
                server_sockaddr = initServerSock(&server_sock,DEFAULT_IP, DEFAULT_PORT);
        }else if(arg==3){
                int convert_port = atoi(argv[2]);				//convert ascall to int
                server_sockaddr = initServerSock(&server_sock,argv[1],convert_port);//argv[1]:ip argv[2]:port
        }
        //startup server thread
        if(pthread_create(&server_thread,NULL,ServerThread,NULL)!=0)printf("无法启动服务端线程\n");

        //process client accept
        while(1){

                if((client_sock = accept(server_sock,(struct sockaddr*)server_sockaddr,&len_of_sockaddr_in))==-1)printf("无法连接\n");

                login_user = Login(client_sock);
                //get user authority
                user_authority = GetUserAuthority(login_user.account_name,login_user.account_password,accounts);

                //        printf("quanxian:%d\n",user_authority);
                //start new thread
                if(user_authority==visitorLevel)
                        strcpy(buf,"游客登陆 ：");
                if (user_authority == rootLevel)
                        strcpy(buf, "管理员登陆 ：");
                if(user_authority==commonLevel)
                        strcpy(buf, "普通用户登陆 ：");
                //get sockfd and authority to client thread
                client_info.client_sockfd=client_sock;
                client_info.authority=user_authority;
                //show login info
                strcat(buf,"您是当前的第  ");

                char buf_count[20];
                //convert number to string
                sprintf(buf_count,"%d",current_accounts+1);
                strcat(buf,buf_count);
                strcat(buf," 个用户");
                send(client_sock,buf,1024,0);

                if(pthread_create(&client_thread,NULL,ClientThread,&client_info)!=0)current_accounts--;
                else current_accounts++;


        }

        close(client_sock);
        close(server_sock);

        return 0;
}

struct sockaddr_in *initServerSock(int *server_sockfd,char *ip,int port) {
        static struct sockaddr_in server_sockaddr;
        //set port and ip
        server_sockaddr.sin_family = AF_INET;
        server_sockaddr.sin_port = htons(port);
        server_sockaddr.sin_addr.s_addr = inet_addr(ip);

        *server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (bind(*server_sockfd, (struct sockaddr*)&server_sockaddr, sizeof(struct sockaddr)) == -1)return;	//bind 
        if (listen(*server_sockfd, 5) == -1)return;//listen
        return &server_sockaddr;
}

void initThreadLists() {
        int j;
        for (j = 0; j<MAX_THREAD; j++) {
                strcpy(thread_lists[j].thread_name, "unused");
                thread_lists[j].thread = 0;
        }
}

ACCOUNT* CreateAccounts(){//create users root user can do everything 
        static ACCOUNT user_lists[1];
        ACCOUNT root;
        strcpy(root.account_name,"root");
        strcpy(root.account_password,"0800");
        root.authority=rootLevel;
        user_lists[0]=root;
        return user_lists;
}

ACCOUNT Login(int client_sock) {
        //user login function return loginer's info
        ACCOUNT login_user;
        char buf[1024];
        strcpy(buf, "请输入名字:");
		if (send(client_sock, buf, 1024, 0) == -1){
				printf("server send error\n");              
				return;
		}
		if (recv(client_sock, buf, 1024, 0) == -1)printf("server receive error in while\n"); 
        strcpy(login_user.account_name, buf);

        strcpy(buf, "请输入密码:");
        if (send(client_sock, buf, 1024, 0) == -1)printf("server send error\n");        
        if (recv(client_sock, buf, 1024, 0) == -1)printf("server receive error in while\n");   
        strcpy(login_user.account_password, buf);
        return login_user;
}

int SplitByTag(char tag,const char *complete_command,char* out_put_command,char *parameter){
        char buf[2][50] = {0,0};
        int counts = strlen(complete_command);
        int sp,tagp = 0;

        for(tagp=0;tagp<counts;tagp++){
                if(complete_command[tagp]==tag)break;
        }
        if(tagp==counts)sp=-1; 
        strncat(buf[0],complete_command,tagp++);

        for(;tagp<counts;tagp++)strncat(buf[1],&complete_command[tagp],1);

        int i=0;
        for(i=0;i<strlen(out_put_command);i++)out_put_command[i]=' ';
        for(i=0;i<strlen(out_put_command);i++)out_put_command[i]=' ';
        memset(out_put_command,0,strlen(out_put_command));
        memset(parameter,0,strlen(parameter));
        strcpy(out_put_command,buf[0]);
        strcpy(parameter,buf[1]);
        return sp;                       
}

void *ServerThread(void *arg){
        //struct UserThreadAndName *lists;
        //lists = (struct UserThreadAndName*)arg;
        pthread_detach(pthread_self());
        int k=0;
        char buffer[1024];
        char command[20];
        char content[20];
        int isSplited=-1;
        printf("已开启服务器\n");
        while(1){

                printf("ftpserver>");
                scanf(" %[^\n]",buffer);
                isSplited = SplitByTag(' ',buffer,command,content);
                if(strcmp(buffer,"count all")==0){
                        printf("历史用户数:%d\n",total_accounts);
                }
                else if(strcmp(buffer,"count current")==0){
                        printf("当前用户数:%d\n",current_accounts);
                }
                else if(strcmp(buffer,"quit")==0){
                        exit(0);  
                }
                else if(strcmp(buffer,"lists")==0){
                        for(k=0;k<20;k++){
                                if(strcmp(thread_lists[k].thread_name,"unused")!=0){
                                        printf("%s,thread id: %lu\n",thread_lists[k].thread_name,thread_lists[k].thread);
                                }
                        }

                }
                else if (strcmp(command, "kill") == 0) {
                        if (isSplited != -1) {
                                for (k = 0; k<40; k++) {
                                        if (strcmp(thread_lists[k].thread_name, content) == 0) {
                                                memset(thread_lists[k].thread_name, 0, 20);
                                                strcpy(thread_lists[k].thread_name, "unused");
                                                printf("kill %s\n", content);
                                        }
                                }
                        }
                }
                else {
                        printf("无法识别的命令\n");
                }

        }

}

void *ClientThread(void *arg){

        char loginer_info[50];
        ClientInfo client_info = *(ClientInfo*)arg;
        int client_sock = client_info.client_sockfd;
        int user_authority = client_info.authority;

        pthread_detach(pthread_self());
        total_accounts++;

        //get usable thread to put client thread
        int  thread_id=0;
        thread_id = GetUnUsedThread();

        if (user_authority == rootLevel)sprintf(loginer_info, "root_user:%d", total_accounts);//total users as the ID
        else sprintf(loginer_info, "visitor:%d", total_accounts);

        strcpy(thread_lists[thread_id].thread_name,loginer_info);
        thread_lists[thread_id].thread = pthread_self();	

        //start doing command
        JudgeAndExecute(client_sock, user_authority,thread_id);

}

int GetUnUsedThread() {
        int thread_id;
        for (thread_id = 0; thread_id<MAX_THREAD; thread_id++) {
                if (strcmp(thread_lists[thread_id].thread_name, "unused") == 0)break;  //unused means it is not used
        }
        return thread_id;
}

int GetUserAuthority(char *account_name,char *account_password,ACCOUNT* user_lists){
        int i,user_kind=-1;
        for(i=0;i<USER_ACCOUNTS;i++)
                if((strcmp(account_name,user_lists[i].account_name)==0)&&(strcmp(account_password,user_lists[i].account_password)==0))
                        return user_lists[i].authority;

        return visitorLevel;
}

int GetCommandKind(char *cmd) {

        if (strcmp(cmd, "pwd") == 0) return PWD;
        else if (strcmp(cmd, "cd") == 0) return CD;
        else if (strcmp(cmd, "ls") == 0) return LS;
        else if (strcmp(cmd, "put") == 0) return PUT;
        else if (strcmp(cmd, "get") == 0) return GET;
        else if (strcmp(cmd, "binary") == 0)return BINARY;
        else if (strcmp(cmd, "ascall") == 0)return ASCALL;
        else if (strcmp(cmd, "mkdir") == 0) return MKDIR;
        else if (strcmp(cmd, "rmdir") == 0) return RMDIR;
        else if (strcmp(cmd, "finish") == 0) return FINISH;
        else return -1;

}

void JudgeAndExecute(int client_sock, int user_authority, int thread_id) {
        char  buf[1024] = { 0 };
        int illegal;
        int buf_len = 50;
        char cmd[80] = { 0 };
        char parameter[80] = { 0 };

        char server_dir[1024] = { 0 };
        char current_dir[1024] = { 0 };
        //set default dir
        strcpy(server_dir, DEFAULT_DIR);
        strcpy(current_dir, server_dir);
        chdir(server_dir);
        while (1) {
                if (recv(client_sock, buf, 1024, 0) == -1)break;
                illegal = SplitByTag(' ', buf, cmd, parameter);
                int ret = GetCommandKind(cmd);
                if(strcmp(buf,"quit")==0){
                    current_accounts--;
                    return;
                }
                //***do command
                switch (ret) {
                        case LS:
                                ExecuteLs(client_sock, current_dir);
                                break;
                        case PWD:
                                getcwd(buf, buf_len);
                                send(client_sock, buf, buf_len, 0);
                                break;
                        case BINARY:
                                binary = 1;
                                strcpy(buf, "当前为二进制传输模式");
                                send(client_sock, buf, buf_len, 0);
                                break;
                        case ASCALL:
                                binary = 0;
                                strcpy(buf, "当前为文本传输模式");
                                send(client_sock, buf, buf_len, 0);
                                break;
                        case RMDIR:
                                if (illegal == -1 || user_authority >= 1) {
                                        strcpy(buf, "无权限");
                                        send(client_sock, buf, buf_len, 0);
                                }
                                else ExecuteRmDir(client_sock, parameter);
                                break;
                        case MKDIR:
                                if (illegal == -1 || user_authority >= 1) {
                                        strcpy(buf, "无权限");
                                        send(client_sock, buf, buf_len, 0);
                                }
                                else ExecuteMkdir(client_sock, parameter);
                                break;
                        case CD:
                                if (illegal == -1 || user_authority >= 1) {
                                        strcpy(buf, "无权限");
                                        send(client_sock, buf, buf_len, 0);
                                }
                                else {
                                        if (strcmp(parameter, "") != 0 && strcmp(parameter, "\n") != 0)
                                                ExecuteCd(client_sock, parameter);
                                }
                                break;
                        case GET:
                                if (illegal == -1||user_authority>=1) {
                                        strcpy(buf, "无权限");
                                        send(client_sock, buf, buf_len, 0);
                                }
                                else {
                                        ExecuteGet(client_sock, parameter);
                                }
                                break;
                        case PUT:
                                if (illegal == -1||user_authority>=1) {
                                        strcpy(buf, "无权限");
                                        send(client_sock, buf, buf_len, 0);
                                }
                                else {
                                        ExecutePut(client_sock, parameter);
                                }
                                break;
                        case FINISH:
                                    strcpy(buf,"传输完毕\n");
                                    send(client_sock,buf,20,0);
                                break;
                        case QUIT:
                                current_accounts--;
                                strcpy(thread_lists[thread_id].thread_name, "unused");
                                close(client_sock);
                                return;//just break only jump out the switch loop
                                break;
                        default:
                                strcpy(buf, "未知命令\n");
                                send(client_sock, buf, 30, 0);
                                break;

                }
           }

}

void ExecuteMkdir(int client_sockfd,char * parameter){
        char buf[40];
        if (mkdir(parameter, S_IXUSR | S_IXOTH | S_IRUSR | S_IWUSR | S_IROTH | S_IWOTH) == -1) {
                strcpy(buf, "创建目录失败");
                send(client_sockfd, buf, 40, 0);
                return;
        }
        strcpy(buf,"目录已创建");

        strcat(buf,parameter);
        send(client_sockfd,buf,40,0);

}

void ExecuteCd(int client_sockfd,char *parameter){
        char buf[40]={0};
        char current_dir[30];
        if (chdir(parameter) == -1) {
                strcpy(buf, "失败");
                send(client_sockfd, buf, 30, 0);
                return;
        }

        strcpy(buf,"完成");
        getcwd(current_dir,30);                          
        strcat(buf,current_dir);    
        send(client_sockfd,buf,30,0);

}

void ExecuteRmDir(int client_sockfd,char *parameter){
        char buf[1024];
        if (rmdir(parameter) == -1) {
                strcpy(buf, "失败");
                send(client_sockfd, buf, 40, 0);
                return;
        }

        strcpy(buf,"完成");

        send(client_sockfd,buf,40,0);
}

void ExecuteLs(int client_sock,char *path){
        char buf[120]={0};
        DIR *dp;
        struct dirent * entry;
        if((dp=opendir(path))==NULL){
                strcpy(buf,"执行失败");
                send(client_sock,buf,120,0);
                return;
        } 
        while((entry=readdir(dp))!=NULL){
                strcat(buf,entry->d_name);
                strcat(buf,"\n");
        }

        closedir(dp);
        send(client_sock,buf,120,0);

}

void ExecuteGet(int client_sock,char *file_name){
        char buf[1024] = {0};
        FILE * fp;
        if(binary==0){
                fp = fopen(file_name,"rt+");
                if(fp==NULL){
                        printf("发送失败\n");
                        strcpy(buf,"no");
                        send(client_sock,buf,10,0);
                        fclose(fp);
                        return;
                }
                //start sending file to client
                strcpy(buf,"ready");
                send(client_sock,buf,1024,0);
                while(1){
                        if(fread(buf,1,1024,fp)==0){
                                memset(buf,0,1024);
                                strcpy(buf,"finish");
                                send(client_sock,buf,20,0);
                                break;
                        }
                        send(client_sock,buf,1024,0);
                }
        }
        if(binary==1){
                fp = fopen(file_name,"rb+");
                if(fp==NULL){
                        strcpy(buf,"no");
                        send(client_sock,buf,10,0);
                        fclose(fp);
                        return;
                }
                //ok to send file
                strcpy(buf,"ready");
                send(client_sock,buf,1024,0);//inform client to start receive
                while(1){
                        memset(buf,0,1024);
                        buf[1023]=fread(buf,sizeof(char),100,fp);
                        if((int)buf[1023]==0){
                                memset(buf,0,1024);
                                strcpy(buf,"finish");
                                send(client_sock,buf,20,0);//translated send ServerEnd
                                break;
                        }
                        //translate file
                        send(client_sock,buf,1024,0);
                }
        }		
        fclose(fp);

}

void ExecutePut(int client_sockfd,char *file){
        FILE *fp;
        char buf[1024]={0};

        if(binary==1){
                fp = fopen(file,"wb+");
                if (fp == NULL) {
                        memset(buf, 0, 1024);
                        strcpy(buf, "can't open");
                        send(client_sockfd, buf, 100, 0);
                        return;
                }
        }
        if(binary==0){
                fp = fopen(file,"wt+");
                if (fp == NULL) {
                        strcpy(buf, "can't open");
                        send(client_sockfd, buf, 100, 0);
                        return;
                }
        }
        //start receive from client
        memset(buf,0,1024);
        strcpy(buf,"ReadyToPutFile");
        send(client_sockfd,buf,80,0);        
        //client start to send to server ready to receive
        while(1){
                recv(client_sockfd,buf,1024,0);//receive data from server
                if(strcmp(buf,"ERROR")==0){
                        recv(client_sockfd,buf,1024,0);//client send failed server recv again to avoid bug
                        fclose(fp);
                        return;
                }else if(strcmp(buf,"translated")==0){
                        strcpy(buf,"文件传输完毕");
                        send(client_sockfd,buf,100,0);
                        fclose(fp);
                        return;//translate ended
                }
                if(binary==0){//under ascall mode
                        int i = 0;
                        while(buf[i++]!=0);
                        fwrite(buf,sizeof(char),i-1,fp);
                        fflush(fp);
                        i=0;
                }
                if(binary==1){//under binary mode
                        int counts = (int)buf[1023];//blocks i should read
                        fwrite(buf,sizeof(char),counts,fp);	
                        //printf("write %d blocks\n",counts);
                        fflush(fp);
                }

        }
        fclose(fp);
}
