#include "network.h"
#include <cstring>
#include <sstream>
#include <vector>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netdb.h>
    #include <errno.h>
    #include <fcntl.h>
    #define closesocket close
    #define SOCKET_ERROR -1
    #define INVALID_SOCKET -1
    typedef int SOCKET;
#endif

NetworkSession::NetworkSession()
    : sock(-1), clientSock(-1), role(NONE), connected(false), localColor(CELL_BLACK) {}

NetworkSession::~NetworkSession() {
    disconnect();
}

bool NetworkSession::initSockets() {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        lastError = "WSAStartup failed";
        return false;
    }
#endif
    return true;
}

void NetworkSession::cleanupSockets() {
#ifdef _WIN32
    WSACleanup();
#endif
}

bool NetworkSession::startServer(int port) {
    initSockets();
    role = SERVER;

    sock = (int)socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { lastError = "Failed to create socket"; role = NONE; return false; }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (::bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        lastError = "Bind failed"; closesocket(sock); sock = -1; role = NONE; return false;
    }

    if (listen(sock, 1) < 0) {
        lastError = "Listen failed"; closesocket(sock); sock = -1; role = NONE; return false;
    }

    // Accept one client (blocking, but we'll set short timeout)
    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);
    clientSock = (int)accept(sock, (struct sockaddr*)&clientAddr, &addrLen);
    if (clientSock < 0) {
        lastError = "Accept failed"; closesocket(sock); sock = -1; role = NONE; return false;
    }

    // Set receive timeout
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500000; // 500ms
    setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    closesocket(sock); // Don't need listener anymore
    sock = -1;
    connected = true;
    localColor = CELL_BLACK; // Server is black
    return true;
}

bool NetworkSession::startListening(int port) {
    initSockets();
    role = SERVER;

    sock = (int)socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { lastError = "Failed to create socket"; role = NONE; return false; }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (::bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        lastError = "Bind failed"; closesocket(sock); sock = -1; role = NONE; return false;
    }

    if (listen(sock, 1) < 0) {
        lastError = "Listen failed"; closesocket(sock); sock = -1; role = NONE; return false;
    }

    // Non-blocking: accept() returns immediately if no client
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif

    connected = false;
    clientSock = -1;
    localColor = CELL_BLACK;
    return true;
}

bool NetworkSession::acceptConnection() {
    if (role != SERVER || sock < 0) return false;

    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);
    clientSock = (int)accept(sock, (struct sockaddr*)&clientAddr, &addrLen);
    if (clientSock < 0) return false;

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500000;
    setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

#ifdef _WIN32
    closesocket(sock);
#else
    closesocket(sock);
#endif
    sock = -1;
    connected = true;
    return true;
}

bool NetworkSession::connectToHost(const std::string& host, int port) {
    initSockets();
    role = CLIENT;

    clientSock = (int)socket(AF_INET, SOCK_STREAM, 0);
    if (clientSock < 0) { lastError = "Failed to create socket"; role = NONE; return false; }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    struct hostent* he = gethostbyname(host.c_str());
    if (he == nullptr) {
        lastError = "DNS lookup failed"; closesocket(clientSock); clientSock = -1; role = NONE; return false;
    }
    memcpy(&addr.sin_addr, he->h_addr, he->h_length);

    if (connect(clientSock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        lastError = "Connection failed"; closesocket(clientSock); clientSock = -1; role = NONE; return false;
    }

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500000;
    setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    connected = true;
    localColor = CELL_WHITE; // Client is white
    return true;
}

void NetworkSession::disconnect() {
    connected = false;
    if (clientSock >= 0) {
#ifdef _WIN32
        closesocket(clientSock);
#else
        closesocket(clientSock);
#endif
        clientSock = -1;
    }
    if (sock >= 0) {
#ifdef _WIN32
        closesocket(sock);
#else
        closesocket(sock);
#endif
        sock = -1;
    }
    role = NONE;
    cleanupSockets();
}

bool NetworkSession::sendRaw(const std::string& data) {
    if (!connected || clientSock < 0) return false;
    uint32_t len = htonl((uint32_t)data.size());
    if (::send(clientSock, (const char*)&len, 4, 0) < 0) return false;
    if (::send(clientSock, data.c_str(), (int)data.size(), 0) < 0) return false;
    return true;
}

bool NetworkSession::recvRaw(std::string& data, int timeoutMs) {
    if (!connected || clientSock < 0) return false;

    // Read 4-byte length prefix
    uint32_t netLen;
    int n = (int)recv(clientSock, (char*)&netLen, 4, MSG_PEEK);
    if (n <= 0) return false;

    uint32_t msgLen = ntohl(netLen);
    if (msgLen > 4096) { disconnect(); return false; }

    // Read full message
    std::vector<char> buf(msgLen + 1);
    n = (int)recv(clientSock, buf.data(), msgLen, 0);
    if (n <= 0) { disconnect(); return false; }

    buf[msgLen] = 0;
    data = buf.data();
    return true;
}

std::string NetworkSession::encodeMessage(NetMessage::Type type, int r, int c,
                                           Cell color, const std::string& text) const {
    switch (type) {
        case NetMessage::MSG_MOVE: {
            char buf[32]; snprintf(buf, sizeof(buf), "MOVE %d,%d", r, c);
            return buf;
        }
        case NetMessage::MSG_UNDO_REQUEST: return "UNDO";
        case NetMessage::MSG_UNDO_ACCEPT: return "OK";
        case NetMessage::MSG_UNDO_REFUSE: return "NO";
        case NetMessage::MSG_RESIGN: return "RSGN";
        case NetMessage::MSG_CHAT: return "CHAT " + text;
        case NetMessage::MSG_HEARTBEAT: return "PING";
        case NetMessage::MSG_GAME_START: {
            char buf[16]; snprintf(buf, sizeof(buf), "START %d", (int)color);
            return buf;
        }
        case NetMessage::MSG_GAME_OVER: {
            char buf[32];
            snprintf(buf, sizeof(buf), "OVER %d %d %d",
                     (int)color, r, c);
            return buf;
        }
        default: return "";
    }
}

NetMessage NetworkSession::parseMessage(const std::string& raw) const {
    NetMessage msg = {NetMessage::MSG_UNKNOWN, -1, -1, CELL_EMPTY, ""};
    if (raw.empty()) return msg;

    if (raw.rfind("MOVE ", 0) == 0) {
        msg.type = NetMessage::MSG_MOVE;
        sscanf(raw.c_str() + 5, "%d,%d", &msg.row, &msg.col);
    } else if (raw == "UNDO") {
        msg.type = NetMessage::MSG_UNDO_REQUEST;
    } else if (raw == "OK") {
        msg.type = NetMessage::MSG_UNDO_ACCEPT;
    } else if (raw == "NO") {
        msg.type = NetMessage::MSG_UNDO_REFUSE;
    } else if (raw == "RSGN") {
        msg.type = NetMessage::MSG_RESIGN;
    } else if (raw.rfind("CHAT ", 0) == 0) {
        msg.type = NetMessage::MSG_CHAT;
        msg.text = raw.substr(5);
    } else if (raw == "PING" || raw == "PONG") {
        msg.type = NetMessage::MSG_HEARTBEAT;
        msg.text = (raw == "PING") ? "PONG" : "PING";
    } else if (raw.rfind("START ", 0) == 0) {
        msg.type = NetMessage::MSG_GAME_START;
        int c; sscanf(raw.c_str() + 6, "%d", &c);
        msg.color = (Cell)c;
    } else if (raw.rfind("OVER ", 0) == 0) {
        msg.type = NetMessage::MSG_GAME_OVER;
        sscanf(raw.c_str() + 5, "%d %d %d", (int*)&msg.color, &msg.row, &msg.col);
    }
    return msg;
}

bool NetworkSession::sendMove(int row, int col) {
    return sendRaw(encodeMessage(NetMessage::MSG_MOVE, row, col, CELL_EMPTY, ""));
}

bool NetworkSession::sendUndoRequest() {
    return sendRaw(encodeMessage(NetMessage::MSG_UNDO_REQUEST, 0, 0, CELL_EMPTY, ""));
}

bool NetworkSession::sendUndoResponse(bool accept) {
    return sendRaw(encodeMessage(accept ? NetMessage::MSG_UNDO_ACCEPT
                                       : NetMessage::MSG_UNDO_REFUSE, 0, 0, CELL_EMPTY, ""));
}

bool NetworkSession::sendResign() {
    return sendRaw(encodeMessage(NetMessage::MSG_RESIGN, 0, 0, CELL_EMPTY, ""));
}

bool NetworkSession::sendChat(const std::string& msg) {
    return sendRaw(encodeMessage(NetMessage::MSG_CHAT, 0, 0, CELL_EMPTY, msg));
}

bool NetworkSession::sendHeartbeat() {
    return sendRaw(encodeMessage(NetMessage::MSG_HEARTBEAT, 0, 0, CELL_EMPTY, ""));
}

bool NetworkSession::recvMessage(NetMessage& msg, int timeoutMs) {
    std::string raw;
    if (!recvRaw(raw, timeoutMs)) return false;
    msg = parseMessage(raw);
    return true;
}

std::string NetworkSession::getConnectionInfo() const {
    if (!connected) return "未连接";
    if (role == SERVER) return "主机模式 (黑方)";
    return "客户端模式 (白方)";
}
