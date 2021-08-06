#include <pthread.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
//#include <unistd.h>  
#include <stdio.h> 
#include <map>
#include <cstring>
#include <iostream>
//#include <readline/readline.h>
//#include <readline/history.h>
#include "msg.h"
#include "config.h"
#include "mdspi.h"
#include "tdspi.h"
#include "util.h"
#include "console.h"
#include "pipe.h"

using namespace std;

// test
char MD_FRONT[64] = "tcp://180.168.146.187:10212";
char TD_FRONT[64] = "tcp://180.168.146.187:10202";

// 24
//char MD_FRONT[] = "tcp://180.168.146.187:10131";
//char TD_FRONT[] = "tcp://180.168.146.187:10130";

char BrokerID[32] = "9999";
char UserID[32] = "188065";
char Password[32] = "a####123456";

//string UserID = "188065";
//string Password = "yungege0";

char AppID[32] = "simnow_client_test";
char AuthCode[32] = "0000000000000000";
char CLIENT_IP[32] = "0.0.0.0";

CThostFtdcMdApi* mdapi = NULL;
CThostFtdcTraderApi* tdapi = NULL;

int md_pipe_fd[2];
int td_pipe_fd[2];
int in_pipe_fd[2];

void init_args(int argc,char* argv[]){
    if(argc == 1){
        return;
    }
    strcpy(MD_FRONT,argv[1]);
    strcpy(TD_FRONT,argv[2]);
    strcpy(BrokerID,argv[3]);
    strcpy(AppID,argv[4]);
    strcpy(AuthCode,argv[5]);
    strcpy(UserID,argv[6]);
    strcpy(Password,argv[7]);

    printf("md_front: %s\n",MD_FRONT);
    printf("td_front: %s\n",TD_FRONT);
    printf("BrokerID: %s\n",BrokerID);
    printf("UserID: %s\n",UserID);
    printf("UserProductInfo: %s\n",AppID);
    printf("AuthCode: %s\n",AuthCode);
	
	    if(argc > 8){
        init_wxy_schedule(argv[8]);
    }
}

void* get_stdin_thread(void*) {
	while (true) {
		char buf[256] = { 0 };
		fgets(buf, 256, stdin);
		buf[strlen(buf)] = '\0';
		buf[strlen(buf)-1] = '\0';
		send(in_pipe_fd[1],buf,256,0);
	}
	return (void*)0;
}

void* md_thread(void*)
{
    mdapi = CThostFtdcMdApi::CreateFtdcMdApi("./",false,false);

    mdapi->Init();
    printf("[info] 行情服务器版本：%s\n",mdapi->GetApiVersion());
    mdapi->RegisterFront(MD_FRONT);

    MdSpi* spi = new MdSpi(md_pipe_fd[1]);
    mdapi->RegisterSpi(spi);

    char* instruments[] = {"jd2109"};
    mdapi->SubscribeForQuoteRsp(instruments,1);

    mdapi->Join();

	return (void*)0;
}

void* td_thread(void*)
{
    tdapi = CThostFtdcTraderApi::CreateFtdcTraderApi("");

    TdSpi* spi = new TdSpi(td_pipe_fd[1]);
    tdapi->RegisterSpi(spi);

    tdapi->RegisterFront(TD_FRONT);

    tdapi->SubscribePublicTopic(THOST_TERT_RESTART);
    tdapi->SubscribePrivateTopic(THOST_TERT_RESTART);

    tdapi->Init();
    printf("[info] 交易服务器版本：%s\n",tdapi->GetApiVersion());

    tdapi->Join();
	return NULL;
}

void* main_thread(void*)
{



    int ret = 0;
    fd_set rset;
    FD_ZERO(&rset);

    int nready;
    int fd_stdin = in_pipe_fd[0];
    int maxfd = fd_stdin;
    if(md_pipe_fd[0] > maxfd)
    {
        maxfd = md_pipe_fd[0];
    }
    if(td_pipe_fd[0] > maxfd)
    {
        maxfd = td_pipe_fd[0];
    }

    int len = 0;
    char readbuf[1024] = {0};
    char writebuf[1024] = {0};
 //   rl_callback_handler_install ("ctp> ", handle_cmd);
    while(1)
    {
        FD_ZERO(&rset);
        FD_SET(fd_stdin, &rset);
        FD_SET(md_pipe_fd[0], &rset);
        FD_SET(td_pipe_fd[0], &rset);

        nready = select(maxfd+1, &rset, NULL, NULL, NULL); // 同时监听标准输入和服务器。
        if(nready == -1)
        {
            perror("select");
            exit(0);
        }
        else if(nready == 0)
        {
            continue;
        }

        if(FD_ISSET(md_pipe_fd[0], &rset)) // 服务器来了数据
        {
            Msg msg;
            ret = recv(md_pipe_fd[0], (char*)&msg, sizeof(msg),0);
            if(ret == 0)
            {
                printf("server close1\n");
                break;
            }
            else if(-1 == ret)
            {
                perror("read1");
                break;
            }
            handle_msg(&msg);
        }
        if(FD_ISSET(td_pipe_fd[0], &rset)) // 服务器来了数据
        {
            Msg msg;
            ret = recv(td_pipe_fd[0], (char*)&msg, sizeof(msg),0);
            if(ret == 0)
            {
                printf("server close1\n");
                break;
            }
            else if(-1 == ret)
            {
                perror("read1");
                break;
            }
            handle_msg(&msg);
        }
        if(FD_ISSET(fd_stdin, &rset)) // 标准输入来了数据就发送给server
        {
            char buff[256]={0};
            ret = recv(fd_stdin, (char*)&buff, sizeof(buff),0);
            if(ret == 0)
            {
                printf("server close1\n");
                break;
            }
            else if(-1 == ret)
            {
                perror("read1");
                break;
            }
            handle_cmd(buff);
        }
    }
}

int main(int argc,char* argv[])
{
	WORD	wVersionRequested;
	WSADATA wsaData;
	int		err, iLen;
	wVersionRequested = MAKEWORD(2, 2);//create 16bit data
//(1)Load WinSock
	err = WSAStartup(wVersionRequested, &wsaData);	//load win socket
	if (err != 0)
	{
		cout << "Load WinSock Failed!";
		return 0;
	}

    init_args(argc,argv);

    pipe(md_pipe_fd);
    pipe(td_pipe_fd);
	pipe(in_pipe_fd);
    pthread_t md_thread_t;
    pthread_t td_thread_t;
    pthread_t main_thread_t;
	pthread_t in_thread_t;
    pthread_create(&md_thread_t,NULL,&md_thread,NULL);
    pthread_create(&td_thread_t,NULL,&td_thread,NULL);
    pthread_create(&main_thread_t,NULL,&main_thread,NULL);
	pthread_create(&in_thread_t, NULL, &get_stdin_thread, NULL);

    int *md_thread_ret=NULL;
    int *td_thread_ret=NULL;

    pthread_join(md_thread_t,(void**)&md_thread_ret);
    pthread_join(td_thread_t,(void**)&td_thread_ret);
    return 0;
}
