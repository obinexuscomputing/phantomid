#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "message.h"
// Internal message queue management
typedef struct MessageQueue {
    Message* messages;
    size_t capacity;
    size_t count;
    size_t head;
    size_t tail;
    pthread_mutex_t lock;
} MessageQueue;

// Message handler registry
typedef struct {
    MessageType type;
    MessageHandlerFn handler;
} HandlerEntry;

typedef struct {
    HandlerEntry* entries;
    size_t count;
    size_t capacity;
    pthread_mutex_t lock;
} HandlerRegistry;

// Message context stored in program user_data
typedef struct {
    MessageQueue incoming;  // Incoming message queue
    MessageQueue outgoing;  // Outgoing message queue
    HandlerRegistry handlers;  // Message handlers
    uint32_t next_msg_id;  // Message ID counter
} MessageContext;

// Queue initialization
static bool init_queue(MessageQueue* queue, size_t capacity) {
    queue->messages = calloc(capacity, sizeof(Message));
    if (!queue->messages) return false;
    
    queue->capacity = capacity;
    queue->count = 0;
    queue->head = 0;
    queue->tail = 0;
    
    if (pthread_mutex_init(&queue->lock, NULL) != 0) {
        free(queue->messages);
        return false;
    }
    
    return true;
}

// Queue cleanup
static void cleanup_queue(MessageQueue* queue) {
    pthread_mutex_lock(&queue->lock);
    free(queue->messages);
    queue->messages = NULL;
    queue->count = 0;
    pthread_mutex_unlock(&queue->lock);
    pthread_mutex_destroy(&queue->lock);
}

// Handler registry initialization
static bool init_registry(HandlerRegistry* registry, size_t capacity) {
    registry->entries = calloc(capacity, sizeof(HandlerEntry));
    if (!registry->entries) return false;
    
    registry->capacity = capacity;
    registry->count = 0;
    
    if (pthread_mutex_init(&registry->lock, NULL) != 0) {
        free(registry->entries);
        return false;
    }
    
    return true;
}

// Handler registry cleanup
static void cleanup_registry(HandlerRegistry* registry) {
    pthread_mutex_lock(&registry->lock);
    free(registry->entries);
    registry->entries = NULL;
    registry->count = 0;
    pthread_mutex_unlock(&registry->lock);
    pthread_mutex_destroy(&registry->lock);
}

// Message registration handler
static bool register_message_handler(Program* program, MessageType type, MessageHandlerFn handler) {
    if (!program || !handler) return false;
    
    MessageContext* ctx = program->user_data;
    HandlerRegistry* registry = &ctx->handlers;
    
    pthread_mutex_lock(&registry->lock);
    
    // Check for existing handler
    for (size_t i = 0; i < registry->count; i++) {
        if (registry->entries[i].type == type) {
            registry->entries[i].handler = handler;
            pthread_mutex_unlock(&registry->lock);
            return true;
        }
    }
    
    // Add new handler if space available
    if (registry->count < registry->capacity) {
        registry->entries[registry->count].type = type;
        registry->entries[registry->count].handler = handler;
        registry->count++;
        pthread_mutex_unlock(&registry->lock);
        return true;
    }
    
    pthread_mutex_unlock(&registry->lock);
    return false;
}

// Message sending implementation
static bool send_message(Program* program, const char* target, const Message* message) {
    if (!program || !target || !message) return false;
    
    MessageContext* ctx = program->user_data;
    MessageQueue* outgoing = &ctx->outgoing;
    
    pthread_mutex_lock(&outgoing->lock);
    
    if (outgoing->count >= outgoing->capacity) {
        pthread_mutex_unlock(&outgoing->lock);
        return false;
    }
    
    // Copy message to outgoing queue
    Message* slot = &outgoing->messages[outgoing->tail];
    memcpy(slot, message, sizeof(Message));
    strncpy(slot->target, target, sizeof(slot->target) - 1);
    slot->id = __sync_fetch_and_add(&ctx->next_msg_id, 1);
    slot->timestamp = time(NULL);
    
    outgoing->tail = (outgoing->tail + 1) % outgoing->capacity;
    outgoing->count++;
    
    pthread_mutex_unlock(&outgoing->lock);
    return true;
}

// Message broadcast implementation
static bool broadcast_message(Program* program, const Message* message) {
    if (!program || !message) return false;
    
    // In the existing codebase, this maps to phantom_message_send with NULL target
    Message broadcast_msg = *message;
    broadcast_msg.target[0] = '\0';  // Empty target means broadcast
    
    return send_message(program, "", &broadcast_msg);
}

// Message forwarding implementation
static bool forward_message(Program* program, const char* target, const Message* message) {
    if (!program || !target || !message) return false;
    
    Message forward_msg = *message;
    strncpy(forward_msg.target, target, sizeof(forward_msg.target) - 1);
    
    return send_message(program, target, &forward_msg);
}

// Message validation
static bool validate_message(Program* program, const Message* message) {
    if (!program || !message) return false;
    
    // Basic validation
    if (message->type >= MSG_CUSTOM && message->type < MSG_SYSTEM) return false;
    if (message->data_size > 0 && !message->data) return false;
    if (message->timestamp > time(NULL)) return false;
    
    return true;
}

// Message encoding
static bool encode_message(Program* program, const Message* message, void* buffer, size_t* size) {
    if (!program || !message || !buffer || !size) return false;
    if (*size < sizeof(Message)) return false;
    
    // Basic encoding - in real implementation would handle endianness, etc.
    memcpy(buffer, message, sizeof(Message));
    *size = sizeof(Message);
    
    return true;
}

// Message decoding
static bool decode_message(Program* program, const void* buffer, size_t size, Message* message) {
    if (!program || !buffer || !message) return false;
    if (size < sizeof(Message)) return false;
    
    // Basic decoding - in real implementation would handle endianness, etc.
    memcpy(message, buffer, sizeof(Message));
    
    return true;
}

// Message interface instance
static MessageInterface message_interface = {
    .register_handler = register_message_handler,
    .send = send_message,
    .broadcast = broadcast_message,
    .forward = forward_message,
    .validate = validate_message,
    .encode = encode_message,
    .decode = decode_message
};

// Message interface initialization
bool message_init(Program* program) {
    MessageContext* ctx = calloc(1, sizeof(MessageContext));
    if (!ctx) return false;
    
    if (!init_queue(&ctx->incoming, 1000) || 
        !init_queue(&ctx->outgoing, 1000) ||
        !init_registry(&ctx->handlers, 100)) {
        free(ctx);
        return false;
    }
    
    program->user_data = ctx;
    return true;
}

// Message interface cleanup
void message_cleanup(Program* program) {
    if (!program) return;
    
    MessageContext* ctx = program->user_data;
    if (ctx) {
        cleanup_queue(&ctx->incoming);
        cleanup_queue(&ctx->outgoing);
        cleanup_registry(&ctx->handlers);
        free(ctx);
    }
}

// Get message interface
const MessageInterface* get_message_interface(void) {
    return &message_interface;
}