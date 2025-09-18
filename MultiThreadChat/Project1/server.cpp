#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <vector>
#include <algorithm>
#include <string>

#pragma comment(lib, "Ws2_32.lib")

#define BUF_SIZE 1024
#define DEFAULT_PORT 54000

struct ClientInfo {
    SOCKET sock;
    int id; // ID tu dong
};

CRITICAL_SECTION cs;
std::vector<ClientInfo> clients;
int clientCounter = 0;

// Gui message den tat ca client tru sender
void Broadcast(const char* msg, int len, SOCKET sender) {
    EnterCriticalSection(&cs);
    for (auto& c : clients) {
        if (c.sock != sender) {
            send(c.sock, msg, len, 0);
        }
    }
    LeaveCriticalSection(&cs);
}

// Thread xu ly tung client
DWORD WINAPI ClientThread(LPVOID param) {
    ClientInfo* ci = (ClientInfo*)param;
    SOCKET s = ci->sock;
    int id = ci->id;

    char buf[BUF_SIZE];

    // Thong bao client moi vao
    char joinMsg[128];
    snprintf(joinMsg, sizeof(joinMsg), "[Server] Client#%d da tham gia.\n", id);
    Broadcast(joinMsg, (int)strlen(joinMsg), s);
    printf("%s", joinMsg);

    while (true) {
        int ret = recv(s, buf, BUF_SIZE - 1, 0);
        if (ret <= 0) {
            printf("Client#%d ngat ket noi\n", id);
            break;
        }
        buf[ret] = 0;

        // Them prefix de phan biet
        char msg[BUF_SIZE + 64];
        snprintf(msg, sizeof(msg), "[Client#%d] %s", id, buf);
        printf("%s", msg);

        Broadcast(msg, (int)strlen(msg), s);
    }

    // Thong bao roi phong
    char leftMsg[128];
    snprintf(leftMsg, sizeof(leftMsg), "[Server] Client#%d da thoat.\n", id);
    Broadcast(leftMsg, (int)strlen(leftMsg), s);
    printf("%s", leftMsg);

    // Xoa khoi danh sach
    EnterCriticalSection(&cs);
    clients.erase(std::remove_if(clients.begin(), clients.end(),
        [s](const ClientInfo& c) { return c.sock == s; }), clients.end());
    LeaveCriticalSection(&cs);

    closesocket(s);
    delete ci;
    return 0;
}

int main() {
    WSADATA wsaData;
    SOCKET listenSock = INVALID_SOCKET;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup that bai\n");
        return 1;
    }

    listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        printf("Tao socket that bai\n");
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(DEFAULT_PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        printf("Bind that bai\n");
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }

    if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR) {
        printf("Listen that bai\n");
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }

    printf("Chat server dang lang nghe tai port %d...\n", DEFAULT_PORT);

    InitializeCriticalSection(&cs);

    while (true) {
        sockaddr_in clientAddr;
        int clientSize = sizeof(clientAddr);
        SOCKET clientSock = accept(listenSock, (sockaddr*)&clientAddr, &clientSize);
        if (clientSock == INVALID_SOCKET) {
            printf("Accept that bai\n");
            continue;
        }

        // Gan ID tu dong
        int id = ++clientCounter;

        printf("Client moi ket noi: %s:%d => Client#%d\n",
            inet_ntoa(clientAddr.sin_addr),
            ntohs(clientAddr.sin_port),
            id);

        ClientInfo* ci = new ClientInfo{ clientSock, id };

        EnterCriticalSection(&cs);
        clients.push_back(*ci);
        LeaveCriticalSection(&cs);

        DWORD tid;
        HANDLE h = CreateThread(NULL, 0, ClientThread, ci, 0, &tid);
        if (!h) {
            printf("Tao thread that bai\n");
            closesocket(clientSock);
            delete ci;
        }
        else {
            CloseHandle(h);
        }
    }

    DeleteCriticalSection(&cs);
    closesocket(listenSock);
    WSACleanup();
    return 0;
}
