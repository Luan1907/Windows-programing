#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <string>
#include <stdlib.h>

#pragma comment(lib, "Ws2_32.lib")

#define BUF_SIZE 1024

struct ThreadParam {
    SOCKET sock;
};

DWORD WINAPI RecvThread(LPVOID param) {
    ThreadParam* p = (ThreadParam*)param;
    SOCKET s = p->sock;
    char buf[BUF_SIZE];
    while (true) {
        int ret = recv(s, buf, BUF_SIZE - 1, 0);
        if (ret <= 0) {
            printf("Server closed connection or error\n");
            break;
        }
        buf[ret] = '\0';
        printf("%s", buf);
    }
    return 0;
}

int main() {
    WSADATA wsaData;
    SOCKET clientSock = INVALID_SOCKET;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }

    clientSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSock == INVALID_SOCKET) {
        printf("socket failed\n");
        WSACleanup();
        return 1;
    }

    char serverIp[64];
    int port;
    printf("Nhap dia chi server (default 127.0.0.1): ");
    if (fgets(serverIp, sizeof(serverIp), stdin) == NULL) {
        strcpy_s(serverIp, sizeof(serverIp), "127.0.0.1\n");
    }
    if (serverIp[0] == '\n') strcpy_s(serverIp, sizeof(serverIp), "127.0.0.1\n");
    // remove newline
    char* pnl = strchr(serverIp, '\n');
    if (pnl) *pnl = 0;

    char portStr[16];
    printf("Nhap port (default 54000): ");
    if (fgets(portStr, sizeof(portStr), stdin) == NULL) {
        strcpy_s(portStr, sizeof(portStr), "54000\n");
    }
    if (portStr[0] == '\n') strcpy_s(portStr, sizeof(portStr), "54000\n");
    pnl = strchr(portStr, '\n');
    if (pnl) *pnl = 0;
    port = atoi(portStr);

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr(serverIp);

    if (connect(clientSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        printf("Could not connect to server %s:%d\n", serverIp, port);
        closesocket(clientSock);
        WSACleanup();
        return 1;
    }

    printf("Connected to %s:%d\n", serverIp, port);

    ThreadParam tp; tp.sock = clientSock;
    DWORD tid;
    HANDLE h = CreateThread(NULL, 0, RecvThread, &tp, 0, &tid);
    if (!h) {
        printf("CreateThread failed\n");
        closesocket(clientSock);
        WSACleanup();
        return 1;
    }

    // main thread: send input
    char line[BUF_SIZE];
    while (true) {
        if (!fgets(line, sizeof(line), stdin)) break;
        int len = (int)strlen(line);
        if (len == 0) continue;
        // allow exit command
        if (strncmp(line, "/quit", 5) == 0) break;
        int sent = send(clientSock, line, len, 0);
        if (sent == SOCKET_ERROR) {
            printf("send failed\n");
            break;
        }
    }

    // cleanup
    closesocket(clientSock);
    // Wait for receiver thread to end
    WaitForSingleObject(h, 2000);
    CloseHandle(h);
    WSACleanup();
    return 0;
}
