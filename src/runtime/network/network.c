#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #define CLOSE_SOCKET closesocket
    #define SOCKET_ERROR_CODE WSAGetLastError()
    #define sleep(x) Sleep(x * 1000)
    #define usleep(x) Sleep(x / 1000)
    typedef SOCKET socket_t;
#else
    #include <unistd.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #define CLOSE_SOCKET close
    #define SOCKET_ERROR_CODE errno
    #define INVALID_SOCKET -1
    typedef int socket_t;
#endif

#define MAX_BACKLOG 10
#define BUFFER_SIZE 4096

// Helper to set socket non-blocking
static bool set_nonblocking(socket_t sock) {
#ifdef _WIN32
    unsigned long mode = 1;
    return ioctlsocket(sock, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) return false;
    return fcntl(sock, F_SETFL, flags | O_NONBLOCK) != -1;
#endif
}

// Helper to set socket options
static bool set_socket_options(socket_t sock) {
    int yes = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void*)&yes, sizeof(yes)) == -1) {
        return false;
    }
    
    struct linger ling = {0, 0}; // Disable linger
    if (setsockopt(sock, SOL_SOCKET, SO_LINGER, (void*)&ling, sizeof(ling)) == -1) {
        return false;
    }
    
    return true;
}

// Create network context
NetworkContext* network_create(uint16_t port) {
    NetworkContext* ctx = calloc(1, sizeof(NetworkContext));
    if (!ctx) return NULL;

    ctx->port = port;
    ctx->max_connections = MAX_CONNECTIONS;
    ctx->connections = calloc(ctx->max_connections, sizeof(NetworkConnection));
    if (!ctx->connections) {
        free(ctx);
        return NULL;
    }

    if (pthread_mutex_init(&ctx->lock, NULL) != 0) {
        free(ctx->connections);
        free(ctx);
        return NULL;
    }

#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2,2), &wsa_data) != 0) {
        free(ctx->connections);
        free(ctx);
        return NULL;
    }
#endif

    return ctx;
}

// Start network server
bool network_start(NetworkContext* ctx) {
    if (!ctx) return false;

    // Create server socket
    ctx->server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (ctx->server_socket == INVALID_SOCKET) {
        return false;
    }

    // Set socket options
    if (!set_socket_options(ctx->server_socket) || !set_nonblocking(ctx->server_socket)) {
        CLOSE_SOCKET(ctx->server_socket);
        return false;
    }

    // Bind socket
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(ctx->port);

    if (bind(ctx->server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        CLOSE_SOCKET(ctx->server_socket);
        return false;
    }

    // Listen for connections
    if (listen(ctx->server_socket, MAX_BACKLOG) == -1) {
        CLOSE_SOCKET(ctx->server_socket);
        return false;
    }

    return true;
}

// Accept new connection
static bool accept_connection(NetworkContext* ctx) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    socket_t client_sock = accept(ctx->server_socket, (struct sockaddr*)&client_addr, &addr_len);
    if (client_sock == INVALID_SOCKET) {
        return false;
    }

    // Set client socket non-blocking
    if (!set_nonblocking(client_sock)) {
        CLOSE_SOCKET(client_sock);
        return false;
    }

    // Find free connection slot
    pthread_mutex_lock(&ctx->lock);
    NetworkConnection* conn = NULL;
    for (size_t i = 0; i < ctx->max_connections; i++) {
        if (!ctx->connections[i].is_active) {
            conn = &ctx->connections[i];
            conn->socket = client_sock;
            conn->is_active = true;
            ctx->active_connections++;
            break;
        }
    }
    pthread_mutex_unlock(&ctx->lock);

    if (!conn) {
        CLOSE_SOCKET(client_sock);
        return false;
    }

    // Notify connection handler
    if (ctx->connect_handler) {
        ctx->connect_handler(ctx, conn);
    }

    return true;
}

// Handle disconnection
static void handle_disconnect(NetworkContext* ctx, NetworkConnection* conn) {
    if (!conn->is_active) return;

    // Notify disconnect handler
    if (ctx->disconnect_handler) {
        ctx->disconnect_handler(ctx, conn);
    }

    pthread_mutex_lock(&ctx->lock);
    CLOSE_SOCKET(conn->socket);
    conn->is_active = false;
    conn->socket = INVALID_SOCKET;
    ctx->active_connections--;
    pthread_mutex_unlock(&ctx->lock);
}

// Poll for network activity
static void poll_connections(NetworkContext* ctx) {
    fd_set readfds;
    struct timeval tv = {0, 100000}; // 100ms timeout
    socket_t max_fd = ctx->server_socket;

    FD_ZERO(&readfds);
    FD_SET(ctx->server_socket, &readfds);

    // Add active connections to set
    pthread_mutex_lock(&ctx->lock);
    for (size_t i = 0; i < ctx->max_connections; i++) {
        if (ctx->connections[i].is_active) {
            FD_SET(ctx->connections[i].socket, &readfds);
            if (ctx->connections[i].socket > max_fd) {
                max_fd = ctx->connections[i].socket;
            }
        }
    }
    pthread_mutex_unlock(&ctx->lock);

    int activity = select(max_fd + 1, &readfds, NULL, NULL, &tv);
    if (activity > 0) {
        // Check for new connections
        if (FD_ISSET(ctx->server_socket, &readfds)) {
            accept_connection(ctx);
        }

        // Check existing connections
        pthread_mutex_lock(&ctx->lock);
        for (size_t i = 0; i < ctx->max_connections; i++) {
            NetworkConnection* conn = &ctx->connections[i];
            if (conn->is_active && FD_ISSET(conn->socket, &readfds)) {
                char buffer[BUFFER_SIZE];
                ssize_t bytes = recv(conn->socket, buffer, sizeof(buffer) - 1, 0);

                if (bytes <= 0) {
                    handle_disconnect(ctx, conn);
                } else if (ctx->message_handler) {
                    NetworkMessage msg = {
                        .type = MSG_DATA,
                        .data_size = bytes
                    };
                    memcpy(msg.data, buffer, bytes);
                    ctx->message_handler(ctx, &msg);
                }
            }
        }
        pthread_mutex_unlock(&ctx->lock);
    }
}

// Send message to specific node
bool network_send(NetworkContext* ctx, const char* node_id, NetworkMessage* msg) {
    if (!ctx || !node_id || !msg) return false;

    bool sent = false;
    pthread_mutex_lock(&ctx->lock);
    
    for (size_t i = 0; i < ctx->max_connections; i++) {
        NetworkConnection* conn = &ctx->connections[i];
        if (conn->is_active && strcmp(conn->node_id, node_id) == 0) {
            ssize_t result = send(conn->socket, msg->data, msg->data_size, 0);
            sent = (result == msg->data_size);
            break;
        }
    }

    pthread_mutex_unlock(&ctx->lock);
    return sent;
}

// Broadcast message to all nodes
bool network_broadcast(NetworkContext* ctx, NetworkMessage* msg) {
    if (!ctx || !msg) return false;

    pthread_mutex_lock(&ctx->lock);
    
    for (size_t i = 0; i < ctx->max_connections; i++) {
        NetworkConnection* conn = &ctx->connections[i];
        if (conn->is_active) {
            send(conn->socket, msg->data, msg->data_size, 0);
        }
    }

    pthread_mutex_unlock(&ctx->lock);
    return true;
}

// Set message handler
void network_set_message_handler(NetworkContext* ctx, MessageHandler handler) {
    if (ctx) ctx->message_handler = handler;
}

// Set connection handler
void network_set_connect_handler(NetworkContext* ctx, ConnectionHandler handler) {
    if (ctx) ctx->connect_handler = handler;
}

// Set disconnection handler
void network_set_disconnect_handler(NetworkContext* ctx, ConnectionHandler handler) {
    if (ctx) ctx->disconnect_handler = handler;
}

// Stop network server
void network_stop(NetworkContext* ctx) {
    if (!ctx) return;

    pthread_mutex_lock(&ctx->lock);
    
    // Close client connections
    for (size_t i = 0; i < ctx->max_connections; i++) {
        if (ctx->connections[i].is_active) {
            CLOSE_SOCKET(ctx->connections[i].socket);
            ctx->connections[i].is_active = false;
        }
    }

    // Close server socket
    if (ctx->server_socket != INVALID_SOCKET) {
        CLOSE_SOCKET(ctx->server_socket);
        ctx->server_socket = INVALID_SOCKET;
    }

    pthread_mutex_unlock(&ctx->lock);
}

// Destroy network context
void network_destroy(NetworkContext* ctx) {
    if (!ctx) return;

    network_stop(ctx);
    
    pthread_mutex_destroy(&ctx->lock);
    free(ctx->connections);
    free(ctx);

#ifdef _WIN32
    WSACleanup();
#endif
}

// Run network loop
void network_run(NetworkContext* ctx) {
    if (!ctx) return;
    poll_connections(ctx);
}