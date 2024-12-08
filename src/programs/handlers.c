#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../interface/program.h"
#include "../interface/message.h"
#include "../interface/command.h"
#include "../runtime/tree/tree.h"
#include "phantomid.h"

// Message handler types
typedef struct {
    NetworkMessage msg;
    TreeNode* source;
    TreeNode* target;
} MessageContext;

// Parse message target/source IDs
static bool parse_message_context(Program* program, const Message* message, MessageContext* ctx) {
    if (!message->source[0]) {
        return false;
    }

    TreeContext* tree = program_get_tree(program);
    ctx->source = tree_find_node(tree, message->source);
    
    if (message->target[0]) {
        ctx->target = tree_find_node(tree, message->target);
        if (!ctx->target) return false;
    }

    return ctx->source != NULL;
}

// Handle node creation message
static bool handle_node_created(Program* program, const Message* message) {
    MessageContext ctx = {0};
    if (!parse_message_context(program, message, &ctx)) {
        return false;
    }

    // Create node under source node
    TreeNode* new_node = tree_create_node(program_get_tree(program), ctx.source->id);
    if (!new_node) return false;

    // Broadcast creation to network
    Message notify = {
        .type = MSG_NODE_CREATED,
        .flags = MSG_FLAG_RELIABLE,
        .data_size = sizeof(new_node->id)
    };
    strncpy(notify.source, new_node->id, sizeof(notify.source));
    memcpy(notify.data, new_node->id, notify.data_size);

    program_get_message(program)->broadcast(program, &notify);
    return true;
}

// Handle node deletion message
static bool handle_node_deleted(Program* program, const Message* message) {
    MessageContext ctx = {0};
    if (!parse_message_context(program, message, &ctx)) {
        return false;
    }

    // Cannot delete root node with children
    if (ctx.source->is_root && ctx.source->child_count > 0) {
        return false;
    }

    // Delete node and handle orphans
    if (!tree_delete_node(program_get_tree(program), ctx.source->id)) {
        return false;
    }

    // Broadcast deletion
    Message notify = {
        .type = MSG_NODE_DELETED,
        .flags = MSG_FLAG_RELIABLE,
        .data_size = sizeof(ctx.source->id)
    };
    strncpy(notify.source, ctx.source->id, sizeof(notify.source));
    memcpy(notify.data, ctx.source->id, notify.data_size);

    program_get_message(program)->broadcast(program, &notify);
    return true;
}

// Handle data message between nodes
static bool handle_node_message(Program* program, const Message* message) {
    MessageContext ctx = {0};
    if (!parse_message_context(program, message, &ctx)) {
        return false;
    }

    // Check if nodes can communicate
    if (!tree_can_communicate(program_get_tree(program), 
                            ctx.source->id, ctx.target->id)) {
        return false;
    }

    // Forward message to target
    Message forward = {
        .type = MSG_DATA,
        .flags = message->flags,
        .data_size = message->data_size
    };
    strncpy(forward.source, ctx.source->id, sizeof(forward.source));
    strncpy(forward.target, ctx.target->id, sizeof(forward.target));
    memcpy(forward.data, message->data, message->data_size);

    program_get_message(program)->send(program, ctx.target->id, &forward);
    return true;
}

// Handle network status message
static bool handle_network_status(Program* program, const Message* message) {
    NetworkState state = {0};
    memcpy(&state, message->data, sizeof(NetworkState));

    // Update network state
    StateEntry entry = {
        .type = PHANTOM_STATE_NETWORK,
        .data = &state,
        .data_size = sizeof(NetworkState)
    };
    strcpy(entry.id, "network");

    return program_get_state(program)->set_entry(program, &entry);
}

// Main message handler
bool phantom_handle_message(Program* program, const Message* message) {
    switch (message->type) {
        case MSG_NODE_CREATED:
            return handle_node_created(program, message);
            
        case MSG_NODE_DELETED:
            return handle_node_deleted(program, message);
            
        case MSG_DATA:
            return handle_node_message(program, message);
            
        case MSG_NETWORK:
            return handle_network_status(program, message);
            
        default:
            return false;
    }
}

// Handle node creation command
static CommandStatus handle_create_command(Program* program, 
                                         const Command* command,
                                         CommandResponse* response) {
    const char* parent_id = command->data_size > 0 ? command->data : NULL;
    
    TreeNode* node = tree_create_node(program_get_tree(program), parent_id);
    if (!node) {
        return CMD_STATUS_ERROR;
    }

    // Copy node ID to response
    response->data = strdup(node->id);
    response->data_size = strlen(node->id) + 1;
    return CMD_STATUS_SUCCESS;
}

// Handle node deletion command
static CommandStatus handle_delete_command(Program* program,
                                         const Command* command,
                                         CommandResponse* response) {
    if (!command->data || command->data_size == 0) {
        return CMD_STATUS_INVALID;
    }

    const char* node_id = command->data;
    if (!tree_delete_node(program_get_tree(program), node_id)) {
        return CMD_STATUS_ERROR;
    }

    return CMD_STATUS_SUCCESS;
}

// Handle status command
static CommandStatus handle_status_command(Program* program,
                                         const Command* command,
                                         CommandResponse* response) {
    TreeContext* tree = program_get_tree(program);
    
    char* status = malloc(1024);
    if (!status) return CMD_STATUS_ERROR;

    snprintf(status, 1024,
            "Nodes: %zu\n"
            "Depth: %zu\n"
            "Root: %s\n",
            tree_get_size(tree),
            tree_get_depth(tree),
            tree_has_root(tree) ? "Present" : "None");

    response->data = status;
    response->data_size = strlen(status) + 1;
    return CMD_STATUS_SUCCESS;
}

// Main command handler
CommandStatus phantom_handle_command(Program* program,
                                   const Command* command,
                                   CommandResponse* response) {
    if (!command || !response) {
        return CMD_STATUS_INVALID;
    }

    switch (command->type) {
        case CMD_NODE:
            if (strcmp(command->source, "create") == 0) {
                return handle_create_command(program, command, response);
            }
            else if (strcmp(command->source, "delete") == 0) {
                return handle_delete_command(program, command, response);
            }
            break;

        case CMD_PROGRAM:
            if (strcmp(command->source, "status") == 0) {
                return handle_status_command(program, command, response);
            }
            break;

        default:
            break;
    }

    return CMD_STATUS_INVALID;
}

// Initialize handlers
bool phantom_handlers_init(Program* program) {
    program_get_message(program)->register_handler(program, MSG_NODE_CREATED, 
                                                 phantom_handle_message);
    program_get_message(program)->register_handler(program, MSG_NODE_DELETED, 
                                                 phantom_handle_message);
    program_get_message(program)->register_handler(program, MSG_DATA,
                                                 phantom_handle_message);
    program_get_message(program)->register_handler(program, MSG_NETWORK,
                                                 phantom_handle_message);

    // Add command handlers to command interface
    program_get_command(program)->register_handler(program, CMD_NODE,
                                                 phantom_handle_command);
    program_get_command(program)->register_handler(program, CMD_PROGRAM,
                                                 phantom_handle_command);

    return true;
}