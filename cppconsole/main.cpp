#include <pthread.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>  
#include <stdio.h> 
#include <map>
#include <cstring>
#include <iostream>
#include "msg.h"
#include "config.h"
#include "mdspi.h"
#include "tdspi.h"
#include "util.h"

using namespace std;

// test
//char MD_FRONT[] = "tcp://180.168.146.187:10211";
//char TD_FRONT[] = "tcp://180.168.146.187:10201";

// 24
char MD_FRONT[] = "tcp://180.168.146.187:10131";
char TD_FRONT[] = "tcp://180.168.146.187:10130";

string BrokerID = "9999";
string UserID = "123616";
string Password = "nanase4ever";

//string UserID = "188065";
//string Password = "yungege0";

string AppID = "simnow_client_test";
string AuthCode = "0000000000000000";
string CLIENT_IP = "0.0.0.0";

CThostFtdcMdApi* mdapi = NULL;
CThostFtdcTraderApi* tdapi = NULL;

int md_pipe_fd[2];
int td_pipe_fd[2];

void* md_thread(void*)
{
    mdapi = CThostFtdcMdApi::CreateFtdcMdApi("/var/ctp",false,false);

    mdapi->Init();

    mdapi->RegisterFront(MD_FRONT);

    MdSpi* spi = new MdSpi(md_pipe_fd[1]);
    mdapi->RegisterSpi(spi);

    char* instruments[] = {"jd2109"};
    mdapi->SubscribeForQuoteRsp(instruments,1);

    mdapi->Join();
}

void* td_thread(void*)
{
    tdapi = CThostFtdcTraderApi::CreateFtdcTraderApi("");

    TdSpi* spi = new TdSpi(td_pipe_fd[2]);
    tdapi->RegisterSpi(spi);

    tdapi->RegisterFront(TD_FRONT);

    tdapi->SubscribePublicTopic(THOST_TERT_RESTART);
    tdapi->SubscribePrivateTopic(THOST_TERT_RESTART);

    tdapi->Init();

    tdapi->Join();
}

std::map<string,CThostFtdcDepthMarketDataField*> market_datas;

void* handle_msg(Msg* msg)
{
    if(msg->id == msg_market_data){
        CThostFtdcDepthMarketDataField* p = (CThostFtdcDepthMarketDataField*)msg->ptr;
        string ins = string(p->InstrumentID);
        auto it = market_datas.find(ins);
        if(it!=market_datas.end()){
            delete it->second;
        }
        market_datas[ins] = p;
        //printf("data haha\n");
    }
}

void handle_cmd(char* cmd)
{
    string scmd(cmd);
    std::vector<std::string> array = splitWithStl(scmd," ");
    std::string c = array[0];
    if(c.compare("show") || c.compare("s")==0){
        for(auto it=market_datas.begin();it!=market_datas.end();++it)
        {
            printf("%s\t%d %d\t%d %d\t%d\n",
                 it->second->InstrumentID,
                 int(it->second->BidPrice1),
                 int(it->second->AskPrice1),
                 int(it->second->HighestPrice),
                 int(it->second->LowestPrice),
                 int(it->second->OpenInterest - it->second->PreOpenInterest));
        }
    }
}

void* main_thread(void*)
{
    int ret = 0;
    fd_set rset;
    FD_ZERO(&rset);

    int nready;
    int fd_stdin = fileno(stdin);
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
            ret = read(md_pipe_fd[0], &msg, sizeof(msg));
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
            memset(writebuf, 0, sizeof(writebuf));
            ret = read(fd_stdin, writebuf, sizeof(writebuf));
            printf("========= %d\n",ret);
            if(ret > 0){
                handle_cmd(writebuf);
            }
        }
    }
}

int main()
{
    pipe(md_pipe_fd);
    pipe(td_pipe_fd);
    pthread_t md_thread_t;
    pthread_t td_thread_t;
    pthread_t main_thread_t;
    pthread_create(&md_thread_t,NULL,&md_thread,NULL);
    pthread_create(&td_thread_t,NULL,&td_thread,NULL);
    pthread_create(&main_thread_t,NULL,&main_thread,NULL);

    int *md_thread_ret=NULL;
    int *td_thread_ret=NULL;

    pthread_join(md_thread_t,(void**)&md_thread_ret);
    pthread_join(td_thread_t,(void**)&td_thread_ret);
    return 0;
}
