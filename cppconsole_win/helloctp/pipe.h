#pragma once
#include <winsock2.h>


int pipe(int fildes[2])
{
    int tcp1, tcp2;
    sockaddr_in name;
    memset(&name, 0, sizeof(name));
    name.sin_family = AF_INET;
    name.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int namelen = sizeof(name);
    tcp1 = tcp2 = -1;

    int tcp = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp == -1){
        goto clean;
    }
    if (bind(tcp, (sockaddr*)&name, namelen) == -1){
        goto clean;
    }
    if (listen(tcp, 5) == -1){
        goto clean;
    }
    if (getsockname(tcp, (sockaddr*)&name, &namelen) == -1){
        goto clean;
    }
    tcp1 = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp1 == -1){
        goto clean;
    }
    if (-1 == connect(tcp1, (sockaddr*)&name, namelen)){
        goto clean;
    }

    tcp2 = accept(tcp, (sockaddr*)&name, &namelen);
    if (tcp2 == -1){
        goto clean;
    }
    if (closesocket(tcp) == -1){
        goto clean;
    }
    fildes[0] = tcp1;
    fildes[1] = tcp2;
    return 0;
clean:
    if (tcp != -1){
        closesocket(tcp);
    }
    if (tcp2 != -1){
        closesocket(tcp2);
    }
    if (tcp1 != -1){
        closesocket(tcp1);
    }
    return -1;
}