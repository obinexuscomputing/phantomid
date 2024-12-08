#ifndef PHANTOMID_H
#define PHANTOMID_H

#include <stdbool.h>
#include "../interface/program.h"
#include "../interface/message.h"
#include "../interface/command.h"
#include "../runtime/tree/tree.h"

// Network configuration
typedef struct {
    uint16_t port;              // Network port
    size_t max_connections;     // Maximum allowed connections
    uint32_t timeout_ms;        // Connection timeout in milliseconds
    size_t backlog;            // Connection backlog size
} NetworkConfig;

// State configuration
typedef struct {
    bool auto_save;            // Enable automatic state saving
    uint32_t save_interval;    // Save interval in seconds
    size_t max_history;        // Maximum state history to keep
    char state_dir[256];       // State directory path
} StateConfig;

// Forward declarations for message context
typedef struct MessageContext MessageContext;

// Program initialization
bool phantom_register(void);

// Message handling
bool phantom_handle_message(Program* program, const Message* message);
bool phantom_handlers_init(Program* program);

// Command handling
CommandStatus phantom_handle_command(Program* program, 
                                   const Command* command,
                                   CommandResponse* response);

// State management
bool phantom_state_init(Program* program);
void phantom_state_cleanup(Program* program);
bool phantom_save_state(Program* program);
bool phantom_check_state(Program* program);

// Component accessors
TreeContext* phantom_get_tree(Program* program);
MessageContext* phantom_get_messages(Program* program);

// Configuration
void phantom_set_verbose(Program* program, bool verbose);
bool phantom_get_verbose(Program* program);

// Network status codes
typedef enum {
    PHANTOM_NET_SUCCESS = 0,
    PHANTOM_NET_ERROR_CONNECT = -1,
    PHANTOM_NET_ERROR_SEND = -2,
    PHANTOM_NET_ERROR_RECEIVE = -3,
    PHANTOM_NET_ERROR_TIMEOUT = -4,
    PHANTOM_NET_ERROR_INVALID = -5
} PhantomNetStatus;

// Message types (extending base types)
typedef enum {
    PHANTOM_MSG_NODE_JOIN = MSG_CUSTOM,     // Node joining network
    PHANTOM_MSG_NODE_LEAVE,                 // Node leaving network
    PHANTOM_MSG_NODE_UPDATE,                // Node state update
    PHANTOM_MSG_NET_STATUS,                 // Network status update
    PHANTOM_MSG_ERROR                       // Error notification
} PhantomMessageType;

// Command types (extending base types)
typedef enum {
    PHANTOM_CMD_NODE_ADD = CMD_CUSTOM,      // Add new node
    PHANTOM_CMD_NODE_REMOVE,                // Remove node
    PHANTOM_CMD_NODE_UPDATE,                // Update node
    PHANTOM_CMD_NET_CONFIG,                 // Configure network
    PHANTOM_CMD_STATE_SAVE,                 // Force state save
    PHANTOM_CMD_STATE_LOAD                  // Force state load
} PhantomCommandType;

// Error codes
typedef enum {
    PHANTOM_ERROR_NONE = 0,
    PHANTOM_ERROR_INIT = -1,
    PHANTOM_ERROR_MEMORY = -2,
    PHANTOM_ERROR_NETWORK = -3,
    PHANTOM_ERROR_STATE = -4,
    PHANTOM_ERROR_INVALID = -5
} PhantomError;

// Node states
typedef enum {
    PHANTOM_NODE_INACTIVE = 0,
    PHANTOM_NODE_ACTIVE = 1,
    PHANTOM_NODE_CONNECTING = 2,
    PHANTOM_NODE_ERROR = 3
} PhantomNodeState;

// Network events (for callbacks)
typedef enum {
    PHANTOM_EVENT_CONNECT = 0,
    PHANTOM_EVENT_DISCONNECT = 1,
    PHANTOM_EVENT_MESSAGE = 2,
    PHANTOM_EVENT_ERROR = 3
} PhantomNetEvent;

// Callback types
typedef void (*PhantomNodeCallback)(const char* node_id, PhantomNodeState state);
typedef void (*PhantomNetworkCallback)(PhantomNetEvent event, void* data);
typedef void (*PhantomStateCallback)(bool success, const char* error);

// Version information
#define PHANTOM_VERSION_MAJOR 1
#define PHANTOM_VERSION_MINOR 0
#define PHANTOM_VERSION_PATCH 0
#define PHANTOM_VERSION_STRING "1.0.0"

// Feature flags
#define PHANTOM_FEATURE_ENCRYPTION 0x01
#define PHANTOM_FEATURE_COMPRESSION 0x02
#define PHANTOM_FEATURE_AUTH 0x04
#define PHANTOM_FEATURE_PERSISTENCE 0x08

// Constants
#define PHANTOM_MAX_ID_LENGTH 64
#define PHANTOM_MAX_MESSAGE_SIZE 4096
#define PHANTOM_DEFAULT_PORT 8888
#define PHANTOM_DEFAULT_TIMEOUT 1000
#define PHANTOM_MAX_RETRIES 3
#define PHANTOM_SAVE_INTERVAL 300

#endif // PHANTOMID_H