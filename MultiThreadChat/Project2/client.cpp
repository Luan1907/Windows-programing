//==============================================================
//  Chương trình: Chat Client TCP (Windows, WinSock2)
//  Mô tả: Kết nối đến server chat, gửi và nhận tin nhắn song song
//  Hỗ trợ các lệnh:
//      /list               - Xem danh sách người dùng online
//      /msg <user> <msg>   - Gửi tin nhắn riêng
//      /quit               - Thoát khỏi chương trình
//
//=========================================================

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <windows.h>
#include <iostream>
#include <string>

#pragma comment(lib, "Ws2_32.lib")

#define BUF_SIZE 1024  // Kích thước bộ đệm nhận/gửi

//------------------------------------------------------
// Cấu trúc chứa thông tin truyền cho luồng nhận tin
//------------------------------------------------------
struct ThreadParam {
    SOCKET sock; // Socket kết nối đến server
};

//------------------------------------------------------
// Hàm chạy trong luồng phụ: nhận tin nhắn từ server
//------------------------------------------------------
DWORD WINAPI RecvThread(LPVOID param) {
    ThreadParam* p = (ThreadParam*)param;
    SOCKET s = p->sock;
    char buf[BUF_SIZE];

    while (true) {
        // Nhận dữ liệu từ server
        int ret = recv(s, buf, BUF_SIZE - 1, 0);
        if (ret <= 0) {
            // Nếu <= 0 nghĩa là server đã ngắt kết nối hoặc lỗi
            std::cout << "Server da dong ket noi hoac bi loi.\n";
            break;
        }

        buf[ret] = '\0';   // Kết thúc chuỗi
        std::cout << buf;  // In nội dung nhận được ra màn hình
    }

    return 0;
}

//------------------------------------------------------
// Chương trình chính
//------------------------------------------------------
int main() {
    WSADATA wsaData;
    SOCKET clientSock = INVALID_SOCKET;

    // --- Khởi tạo thư viện WinSock ---
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cout << "WSAStartup failed\n";
        return 1;
    }

    // --- Tạo socket TCP ---
    clientSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSock == INVALID_SOCKET) {
        std::cout << "Tao socket that bai\n";
        WSACleanup();
        return 1;
    }

    // --- Nhập địa chỉ server ---
    std::string serverIp;
    std::cout << "Nhap dia chi server (mac dinh 127.0.0.1): ";
    std::getline(std::cin, serverIp);
    if (serverIp.empty()) serverIp = "127.0.0.1";

    // --- Nhập cổng ---
    std::string portStr;
    std::cout << "Nhap port (mac dinh 54000): ";
    std::getline(std::cin, portStr);
    int port = portStr.empty() ? 54000 : std::stoi(portStr);

    // --- Thiết lập thông tin địa chỉ server ---
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr(serverIp.c_str());

    // --- Kết nối tới server ---
    if (connect(clientSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cout << "Khong the ket noi toi server " << serverIp << ":" << port << "\n";
        closesocket(clientSock);
        WSACleanup();
        return 1;
    }

    std::cout << "Da ket noi toi " << serverIp << ":" << port << "\n";
    std::cout << "Lenh ho tro:\n"
        << "  /list                      - xem danh sach nguoi dung online\n"
        << "  /msg <username> <noidung>  - gui tin rieng\n"
        << "  /quit                      - thoat\n\n";

    //------------------------------------------------------
    // Tạo thread phụ để nhận tin nhắn từ server
    //------------------------------------------------------
    ThreadParam tp{ clientSock };
    DWORD tid;
    HANDLE h = CreateThread(NULL, 0, RecvThread, &tp, 0, &tid);
    if (!h) {
        std::cout << "Tao thread that bai\n";
        closesocket(clientSock);
        WSACleanup();
        return 1;
    }

    //------------------------------------------------------
    // Luồng chính: đọc lệnh/tin nhắn từ người dùng và gửi đi
    //------------------------------------------------------
    std::string line;
    while (true) {
        std::getline(std::cin, line);  // Nhập từ bàn phím
        if (line.empty()) continue;    // Bỏ qua dòng trống

        // Nếu nhập /quit thì thoát chương trình
        if (line.rfind("/quit", 0) == 0)
            break;

        // Gửi dữ liệu tới server
        send(clientSock, line.c_str(), (int)line.size(), 0);
    }

    //------------------------------------------------------
    // Dọn dẹp tài nguyên trước khi thoát
    //------------------------------------------------------
    closesocket(clientSock);
    WaitForSingleObject(h, 2000); // Chờ luồng nhận tin dừng lại
    CloseHandle(h);
    WSACleanup();

    std::cout << "Da thoat chuong trinh.\n";
    return 0;
}
