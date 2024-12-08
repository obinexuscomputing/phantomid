#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "command.h"

// Command handler registry entry
typedef struct {
    CommandType type;
    CommandHandlerFn handler;
} HandlerEntry;

// Command handler registry
typedef struct {
    HandlerEntry* entries;
    size_t capacity;
    size_t count;
    pthread_mutex_t lock;
} HandlerRegistry;

// Command context stored in program user_data
typedef struct {
    HandlerRegistry handlers;
    uint32_t next_cmd_id;
    pthread_mutex_t lock;
} CommandContext;

// Initialize handler registry
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

// Cleanup handler registry
static void cleanup_registry(HandlerRegistry* registry) {
    pthread_mutex_lock(&registry->lock);
    free(registry->entries);
    registry->entries = NULL;
    registry->count = 0;
    pthread_mutex_unlock(&registry->lock);
    pthread_mutex_destroy(&registry->lock);
}

// Register command handler
static bool register_handler(Program* program, CommandType type, CommandHandlerFn handler) {
    if (!program || !handler) return false;

    CommandContext* ctx = program->user_data;
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

// Execute command
static CommandStatus execute_command(Program* program, 
                                   const Command* command,
                                   CommandResponse* response) {
    if (!program || !command || !response) {
        return CMD_STATUS_INVALID;
    }

    CommandContext* ctx = program->user_data;
    HandlerRegistry* registry = &ctx->handlers;
    CommandStatus status = CMD_STATUS_INVALID;

    pthread_mutex_lock(&registry->lock);

    // Find handler for command type
    for (size_t i = 0; i < registry->count; i++) {
        if (registry->entries[i].type == command->type) {
            CommandHandlerFn handler = registry->entries[i].handler;
            pthread_mutex_unlock(&registry->lock);
            
            // Initialize response
            response->command_id = command->id;
            response->status = CMD_STATUS_ERROR;
            response->data = NULL;
            response->data_size = 0;

            // Execute handler
            status = handler(program, command, response);
            return status;
        }
    }

    pthread_mutex_unlock(&registry->lock);
    return CMD_STATUS_INVALID;
}

// Route command to target
static bool route_command(Program* program,
                         const char* target,
                         const Command* command) {
    if (!program || !target || !command) return false;

    CommandContext* ctx = program->user_data;
    HandlerRegistry* registry = &ctx->handlers;
    bool routed = false;

    pthread_mutex_lock(&registry->lock);

    // Find handler for command type
    for (size_t i = 0; i < registry->count; i++) {
        if (registry->entries[i].type == command->type) {
            CommandHandlerFn handler = registry->entries[i].handler;
            pthread_mutex_unlock(&registry->lock);

            // Create response structure
            CommandResponse response = {
                .command_id = command->id,
                .status = CMD_STATUS_ERROR,
                .data = NULL,
                .data_size = 0
            };

            // Execute handler and check status
            CommandStatus status = handler(program, command, &response);
            routed = (status == CMD_STATUS_SUCCESS);

            // Cleanup response data if allocated
            if (response.data) {
                free(response.data);
            }

            return routed;
        }
    }

    pthread_mutex_unlock(&registry->lock);
    return false;
}

// Validate command
static bool validate_command(Program* program, const Command* command) {
    if (!program || !command) return false;

    // Basic validation
    if (command->type >= CMD_CUSTOM && command->type < CMD_PROGRAM) {
        return false;
    }

    if (command->data_size > 0 && !command->data) {
        return false;
    }

    // Check source and target
    if (!command->source[0]) {
        return false;
    }

    // Type-specific validation could be added here
    switch (command->type) {
        case CMD_PROGRAM:
        case CMD_NODE:
        case CMD_NETWORK:
        case CMD_STATE:
        case CMD_ADMIN:
            return true;

        default:
            return false;
    }
}

// Command interface instance
static CommandInterface command_interface = {
    .register_handler = register_handler,
    .execute = execute_command,
    .route = route_command,
    .validate = validate_command
};

// Initialize command interface
bool command_init(Program* program) {
    CommandContext* ctx = calloc(1, sizeof(CommandContext));
    if (!ctx) return false;

    if (!init_registry(&ctx->handlers, 100) ||
        pthread_mutex_init(&ctx->lock, NULL) != 0) {
        free(ctx);
        return false;
    }

    program->user_data = ctx;
    return true;
}

// Cleanup command interface
void command_cleanup(Program* program) {
    if (!program) return;

    CommandContext* ctx = program->user_data;
    if (ctx) {
        cleanup_registry(&ctx->handlers);
        pthread_mutex_destroy(&ctx->lock);
        free(ctx);
    }
}

// Get command interface
const CommandInterface* get_command_interface(void) {
    return &command_interface;
}