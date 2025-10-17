#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <windows.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include <sstream>

#pragma comment(lib, "Ws2_32.lib")

#define BUF_SIZE 1024
#define DEFAULT_PORT 54000

// Cấu trúc lưu thông tin một client
struct ClientInfo {
    SOCKET sock;           // Socket của client
    int id;                // Mã định danh client
    std::string username;  // Tên người dùng
};

CRITICAL_SECTION cs;           // Dùng để đồng bộ truy cập danh sách clients
std::vector<ClientInfo*> clients; // Danh sách client đang kết nối
int clientCounter = 0;            // Đếm số client (tăng dần để gán ID)

//------------------------------------------------------
// Hàm gửi tin nhắn đến tất cả client trừ người gửi
//------------------------------------------------------
void Broadcast(const std::string& msg, SOCKET sender) {
    EnterCriticalSection(&cs);
    for (auto* c : clients) {
        if (c->sock != sender) {
            send(c->sock, msg.c_str(), (int)msg.size(), 0);
        }
    }
    LeaveCriticalSection(&cs);
}

//------------------------------------------------------
// Gửi tin nhắn riêng (private) đến client có username cụ thể
//------------------------------------------------------
bool SendPrivateMessage(const std::string& targetName, const std::string& msg) {
    EnterCriticalSection(&cs);
    for (auto* c : clients) {
        // So sánh tên không phân biệt hoa thường
        if (_stricmp(c->username.c_str(), targetName.c_str()) == 0) {
            send(c->sock, msg.c_str(), (int)msg.size(), 0);
            LeaveCriticalSection(&cs);
            return true;
        }
    }
    LeaveCriticalSection(&cs);
    return false;
}

//------------------------------------------------------
// Lấy danh sách người dùng đang online
//------------------------------------------------------
std::string GetOnlineList() {
    std::ostringstream oss;
    oss << "[Server] Danh sach nguoi dung online:\n";
    EnterCriticalSection(&cs);
    for (auto* c : clients) {
        oss << " - " << (c->username.empty() ? "(dang ket noi...)" : c->username) << "\n";
    }
    LeaveCriticalSection(&cs);
    return oss.str();
}

//------------------------------------------------------
// Tạo tên người dùng duy nhất (tránh trùng lặp)
//------------------------------------------------------
std::string MakeUniqueUsername(const std::string& baseName) {
    std::string name = baseName;
    int suffix = 1;

    EnterCriticalSection(&cs);
    // Nếu trùng thì thêm số vào cuối tên
    while (std::any_of(clients.begin(), clients.end(),
        [&](ClientInfo* c) { return _stricmp(c->username.c_str(), name.c_str()) == 0; })) {
        name = baseName + std::to_string(suffix++);
    }
    LeaveCriticalSection(&cs);

    return name;
}

//------------------------------------------------------
// Thread xử lý cho từng client riêng biệt
//------------------------------------------------------
DWORD WINAPI ClientThread(LPVOID param) {
    ClientInfo* ci = (ClientInfo*)param;
    SOCKET s = ci->sock;
    int id = ci->id;
    char buf[BUF_SIZE];

    // --- Gửi yêu cầu nhập username ---
    std::string prompt = "[Server] Nhap ten nguoi dung: ";
    send(s, prompt.c_str(), (int)prompt.size(), 0);

    int ret = recv(s, buf, BUF_SIZE - 1, 0);
    if (ret <= 0) {
        closesocket(s);
        delete ci;
        return 0;
    }

    buf[ret] = 0;
    std::string username(buf);

    // Xóa ký tự xuống dòng, carriage return
    username.erase(std::remove(username.begin(), username.end(), '\r'), username.end());
    username.erase(std::remove(username.begin(), username.end(), '\n'), username.end());

    // Gán username duy nhất
    ci->username = MakeUniqueUsername(username);

    // Gửi xác nhận lại cho client
    std::ostringstream confirm;
    confirm << "[Server] Ten duoc dat la: " << ci->username << "\n";
    send(s, confirm.str().c_str(), (int)confirm.str().size(), 0);

    // Thông báo đến các client khác
    std::ostringstream joinMsg;
    joinMsg << "[Server] " << ci->username << " da tham gia.\n";
    Broadcast(joinMsg.str(), s);
    std::cout << joinMsg.str();

    //------------------------------------------------------
    // Vòng lặp chính: nhận và xử lý tin nhắn từ client
    //------------------------------------------------------
    while (true) {
        int ret = recv(s, buf, BUF_SIZE - 1, 0);
        if (ret <= 0) {
            // Client đóng kết nối
            std::cout << ci->username << " ngat ket noi.\n";
            break;
        }

        buf[ret] = 0;
        std::string input(buf);

        // --- Lệnh xem danh sách người dùng ---
        if (input.rfind("/list", 0) == 0) {
            std::string listMsg = GetOnlineList();
            send(s, listMsg.c_str(), (int)listMsg.size(), 0);
            continue;
        }

        // --- Lệnh gửi tin nhắn riêng ---
        else if (input.rfind("/msg ", 0) == 0) {
            std::istringstream iss(input.substr(5));
            std::string targetName;
            iss >> targetName;
            std::string message;
            std::getline(iss, message);

            // Kiểm tra cú pháp hợp lệ
            if (targetName.empty() || message.empty()) {
                std::string err = "[Server] Sai cu phap. Dung: /msg <username> <noi_dung>\n";
                send(s, err.c_str(), (int)err.size(), 0);
                continue;
            }

            // Gửi tin nhắn riêng
            std::ostringstream msg;
            msg << "(Private tu " << ci->username << "):" << message << "\n";
            if (!SendPrivateMessage(targetName, msg.str())) {
                std::ostringstream err;
                err << "[Server] Nguoi dung '" << targetName << "' khong ton tai.\n";
                send(s, err.str().c_str(), (int)err.str().size(), 0);
            }
            continue;
        }

        // --- Nếu không phải lệnh: coi là tin nhắn công khai ---
        std::ostringstream msg;
        msg << "[" << ci->username << "] " << input;
        std::cout << msg.str();
        Broadcast(msg.str(), s);
    }

    //------------------------------------------------------
    // Khi client thoát: xóa khỏi danh sách và thông báo
    //------------------------------------------------------
    std::ostringstream leftMsg;
    leftMsg << "[Server] " << ci->username << " da thoat.\n";
    Broadcast(leftMsg.str(), s);
    std::cout << leftMsg.str();

    // Xóa client khỏi danh sách
    EnterCriticalSection(&cs);
    clients.erase(std::remove(clients.begin(), clients.end(), ci), clients.end());
    LeaveCriticalSection(&cs);

    closesocket(s);
    delete ci;
    return 0;
}

//------------------------------------------------------
// Hàm main: khởi tạo server, lắng nghe và tạo thread cho mỗi client
//------------------------------------------------------
int main() {
    WSADATA wsaData;
    SOCKET listenSock = INVALID_SOCKET;

    // --- Khởi tạo thư viện WinSock ---
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cout << "WSAStartup that bai\n";
        return 1;
    }

    // --- Nhập port từ người dùng ---
    int port = DEFAULT_PORT;
    std::cout << "Nhap port de khoi dong server (mac dinh " << DEFAULT_PORT << "): ";
    std::string portStr;
    std::getline(std::cin, portStr);
    if (!portStr.empty()) port = std::stoi(portStr);

    // --- Tạo socket lắng nghe ---
    listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        std::cout << "Tao socket that bai\n";
        WSACleanup();
        return 1;
    }

    // --- Gán địa chỉ và cổng ---
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // --- Bind socket ---
    if (bind(listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cout << "Bind that bai (port co the dang su dung)\n";
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }

    // --- Bắt đầu lắng nghe kết nối ---
    if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR) {
        std::cout << "Listen that bai\n";
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }

    std::cout << "Chat server dang lang nghe tai port " << port << "...\n";

    InitializeCriticalSection(&cs); // Khởi tạo đối tượng đồng bộ

    //------------------------------------------------------
    // Vòng lặp chính: chấp nhận client mới
    //------------------------------------------------------
    while (true) {
        sockaddr_in clientAddr;
        int clientSize = sizeof(clientAddr);
        SOCKET clientSock = accept(listenSock, (sockaddr*)&clientAddr, &clientSize);
        if (clientSock == INVALID_SOCKET) {
            std::cout << "Accept that bai\n";
            continue;
        }

        int id = ++clientCounter;
        std::cout << "Client moi ket noi: "
            << inet_ntoa(clientAddr.sin_addr) << ":"
            << ntohs(clientAddr.sin_port)
            << " => Client#" << id << "\n";

        // Cấp phát cấu trúc thông tin client
        ClientInfo* ci = new ClientInfo{ clientSock, id, "" };

        // Lưu client vào danh sách chung
        EnterCriticalSection(&cs);
        clients.push_back(ci);
        LeaveCriticalSection(&cs);

        // --- Tạo luồng riêng để xử lý client ---
        DWORD tid;
        HANDLE h = CreateThread(NULL, 0, ClientThread, ci, 0, &tid);
        if (!h) {
            std::cout << "Tao thread that bai\n";
            closesocket(clientSock);
            delete ci;
        }
        else {
            CloseHandle(h);
        }
    }

    // --- Giải phóng tài nguyên ---
    DeleteCriticalSection(&cs);
    closesocket(listenSock);
    WSACleanup();
    return 0;
}
