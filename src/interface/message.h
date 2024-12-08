#ifndef MESSAGE_INTERFACE_H
#define MESSAGE_INTERFACE_H

#include <time.h>
#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include "program.h"

// Message types
typedef enum {
    MSG_SYSTEM = 0,     // System messages
    MSG_NODE,           // Node-related messages
    MSG_NETWORK,        // Network status messages
    MSG_STATE,          // State changes
    MSG_DATA,           // Generic data messages
    MSG_CUSTOM = 1000   // Base for program-specific messages
} MessageType;

// Message flags
typedef enum {
    MSG_FLAG_NONE = 0,
    MSG_FLAG_RELIABLE = 1,     // Guaranteed delivery
    MSG_FLAG_ORDERED = 2,      // In-order delivery
    MSG_FLAG_ENCRYPTED = 4,    // Content encryption
    MSG_FLAG_COMPRESSED = 8    // Content compression
} MessageFlags;

// Message structure
typedef struct {
    MessageType type;          // Message type
    uint32_t id;              // Message identifier
    MessageFlags flags;        // Message flags
    char source[64];          // Source identifier
    char target[64];          // Target identifier
    uint64_t timestamp;       // Message timestamp
    void* data;               // Message payload
    size_t data_size;        // Payload size
} Message;

// Message handler callback
typedef bool (*MessageHandlerFn)(Program* program, 
                               const Message* message);

// Message interface
typedef struct {
    // Message registration
    bool (*register_handler)(Program* program,
                           MessageType type,
                           MessageHandlerFn handler);
    
    // Message operations
    bool (*send)(Program* program,
                const char* target,
                const Message* message);
                
    bool (*broadcast)(Program* program,
                     const Message* message);
    
    bool (*forward)(Program* program,
                   const char* target,
                   const Message* message);
    
    // Message validation
    bool (*validate)(Program* program,
                    const Message* message);
    
    // Message transformation
    bool (*encode)(Program* program,
                  const Message* message,
                  void* buffer,
                  size_t* size);
                  
    bool (*decode)(Program* program,
                  const void* buffer,
                  size_t size,
                  Message* message);
} MessageInterface;

#endif // MESSAGE_INTERFACE_H