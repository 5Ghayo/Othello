#ifndef OTHELLO_NETWORK_H
#define OTHELLO_NETWORK_H

#include "board.h"
#include <string>
#include <functional>

// Network protocol messages (simple text-based)
struct NetMessage {
    enum Type {
        MSG_MOVE,            // r,c
        MSG_UNDO_REQUEST,    // --
        MSG_UNDO_ACCEPT,     // --
        MSG_UNDO_REFUSE,     // --
        MSG_RESIGN,          // --
        MSG_CHAT,            // text
        MSG_HEARTBEAT,       // --
        MSG_GAME_START,      // color
        MSG_GAME_OVER,       // winner,score
        MSG_UNKNOWN
    };
    Type type;
    int row, col;
    Cell color;
    std::string text;
};

class NetworkSession {
public:
    enum Role { NONE, SERVER, CLIENT };

    NetworkSession();
    ~NetworkSession();

   // Server: start listening on port
   bool startServer(int port = 9876);
   // Async server: start listening without blocking
   bool startListening(int port = 9876);
   // Non-blocking accept, returns true when a client connects
   bool acceptConnection();

   // Client: connect to host:port
   bool connectToHost(const std::string& host, int port = 9876);
   // Close connection
   void disconnect();

    bool isConnected() const { return connected; }
    Role getRole() const { return role; }
    Cell getLocalColor() const { return localColor; }
    void setLocalColor(Cell c) { localColor = c; }
    std::string getLastError() const { return lastError; }

    // Send messages
    bool sendMove(int row, int col);
    bool sendUndoRequest();
    bool sendUndoResponse(bool accept);
    bool sendResign();
    bool sendChat(const std::string& msg);
    bool sendHeartbeat();

    // Receive next message (non-blocking with timeout)
    // Returns true if a message was received
    bool recvMessage(NetMessage& msg, int timeoutMs = 100);

    // Get connection info for display
    std::string getConnectionInfo() const;

private:
    int sock;           // Server socket fd
    int clientSock;     // Client socket fd
    Role role;
    bool connected;
    Cell localColor;
    std::string lastError;

    // Platform-specific socket handling
    bool initSockets();
    void cleanupSockets();

    // Raw send/recv with length prefix
    bool sendRaw(const std::string& data);
    bool recvRaw(std::string& data, int timeoutMs);

    // Parse a received message
    NetMessage parseMessage(const std::string& raw) const;
    // Encode a message to string format
    std::string encodeMessage(NetMessage::Type type, int r, int c, Cell color, const std::string& text) const;
};

#endif
