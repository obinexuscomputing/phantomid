#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>
#include <stdbool.h>

// Network message types
typedef enum {
    MSG_CONNECT,         // New connection
    MSG_DISCONNECT,      // Connection closed
    MSG_NODE_CREATED,    // New node created
    MSG_NODE_DELETED,    // Node deleted
    MSG_NODE_UPDATED,    // Node state changed
    MSG_DATA            // Generic data message
} MessageType;

// Network message structure
typedef struct {
    MessageType type;            // Message type
    char source_id[64];         // Source node ID
    char target_id[64];         // Target node ID 
    uint32_t data_size;         // Size of data
    uint8_t data[];             // Flexible array for message data
} NetworkMessage;

// Network connection state
typedef struct NetworkConnection {
    int socket;                  // Connection socket
    bool is_active;             // Connection status
    char node_id[64];           // Associated node ID
    void* user_data;            // Custom data attachment
} NetworkConnection;

// Network context managing all connections
typedef struct NetworkContext {
    int server_socket;           // Server listening socket
    uint16_t port;              // Server port
    NetworkConnection* connections; // Array of connections
    size_t max_connections;      // Maximum allowed connections
    size_t active_connections;   // Current active connections
    pthread_mutex_t lock;        // Thread safety lock
} NetworkContext;

// Network callbacks
typedef void (*MessageHandler)(NetworkContext* ctx, NetworkMessage* msg);
typedef void (*ConnectionHandler)(NetworkContext* ctx, NetworkConnection* conn);

// Basic network operations
NetworkContext* network_create(uint16_t port);
void network_destroy(NetworkContext* ctx);

// Connection operations
bool network_start(NetworkContext* ctx);
void network_stop(NetworkContext* ctx);
bool network_send(NetworkContext* ctx, const char* node_id, NetworkMessage* msg);
bool network_broadcast(NetworkContext* ctx, NetworkMessage* msg);

// Set handlers
void network_set_message_handler(NetworkContext* ctx, MessageHandler handler);
void network_set_connect_handler(NetworkContext* ctx, ConnectionHandler handler);
void network_set_disconnect_handler(NetworkContext* ctx, ConnectionHandler handler);

#endif // NETWORK_H