#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../interface/program.h"
#include "../interface/message.h"
#include "../interface/command.h"
#include "../interface/state.h"
#include "../runtime/tree/tree.h"
#include "../runtime/network/network.h"
#include "phantomid.h"

// Program user data structure
typedef struct {
    TreeContext* tree;
    MessageContext* messages;
    NetworkConfig network_config;
    StateConfig state_config;
    bool verbose_logging;
} PhantomIDContext;

// Initialize program configuration
static bool init_config(PhantomIDContext* context) {
    // Network configuration
    context->network_config.port = 8888;
    context->network_config.max_connections = 1000;
    context->network_config.timeout_ms = 1000;
    context->network_config.backlog = 10;

    // State configuration
    context->state_config.auto_save = true;
    context->state_config.save_interval = 300;  // 5 minutes
    context->state_config.max_history = 10;
    strncpy(context->state_config.state_dir, "state", sizeof(context->state_config.state_dir));

    return true;
}

// Initialize program components
static bool init_components(Program* program, PhantomIDContext* context) {
    // Initialize tree
    context->tree = tree_create();
    if (!context->tree) return false;

    // Initialize message system
    context->messages = message_create();
    if (!context->messages) {
        tree_destroy(context->tree);
        return false;
    }

    return true;
}

// Program initialization
static bool phantom_init(Program* program) {
    PhantomIDContext* context = calloc(1, sizeof(PhantomIDContext));
    if (!context) return false;

    // Initialize configuration
    if (!init_config(context)) {
        free(context);
        return false;
    }

    // Initialize components
    if (!init_components(program, context)) {
        free(context);
        return false;
    }

    // Initialize handlers
    if (!phantom_handlers_init(program)) {
        message_destroy(context->messages);
        tree_destroy(context->tree);
        free(context);
        return false;
    }

    // Initialize state management
    if (!phantom_state_init(program)) {
        message_destroy(context->messages);
        tree_destroy(context->tree);
        free(context);
        return false;
    }

    program->user_data = context;
    return true;
}

// Cleanup program resources
static void phantom_cleanup(Program* program) {
    if (!program) return;

    PhantomIDContext* context = program->user_data;
    if (context) {
        // Save state before cleanup
        phantom_save_state(program);
        
        // Cleanup state management
        phantom_state_cleanup(program);

        // Cleanup components
        message_destroy(context->messages);
        tree_destroy(context->tree);
        
        free(context);
        program->user_data = NULL;
    }
}

// Program main loop
static void phantom_run(Program* program) {
    PhantomIDContext* context = program->user_data;

    // Check for state save
    phantom_check_state(program);

    // Process messages
    message_process_queue(context->messages);

    // Update network status
    if (context->verbose_logging) {
        network_print_status(program_get_network(program));
    }

    // Small sleep to prevent CPU spinning
    usleep(10000); // 10ms
}

// Message handling wrapper
static bool phantom_handle_message_wrapper(Program* program, const void* message, size_t size) {
    const Message* msg = message;
    
    if (size < sizeof(Message) || !validate_message(msg)) {
        return false;
    }

    return phantom_handle_message(program, msg);
}

// Command handling wrapper
static bool phantom_handle_command_wrapper(Program* program, const char* command, void* response) {
    Command cmd = {0};
    if (!parse_command(command, &cmd)) {
        return false;
    }

    return phantom_handle_command(program, &cmd, response) == CMD_STATUS_SUCCESS;
}

// PhantomID program interface
static const ProgramInterface phantom_interface = {
    .name = "PhantomID",
    .version = "1.0.0",
    .interface_version = 1,
    
    // Lifecycle methods
    .init = phantom_init,
    .cleanup = phantom_cleanup,
    .run = phantom_run,
    
    // Message and command handling
    .handle_message = phantom_handle_message_wrapper,
    .handle_command = phantom_handle_command_wrapper,
    
    // Runtime requirements
    .requirements = {
        .needs_network = true,
        .needs_persistence = true,
        .needs_cli = true,
        .default_port = 8888,
        .max_connections = 1000
    }
};

// Program accessors
TreeContext* phantom_get_tree(Program* program) {
    PhantomIDContext* context = program->user_data;
    return context ? context->tree : NULL;
}

MessageContext* phantom_get_messages(Program* program) {
    PhantomIDContext* context = program->user_data;
    return context ? context->messages : NULL;
}

// Configuration getters/setters
void phantom_set_verbose(Program* program, bool verbose) {
    PhantomIDContext* context = program->user_data;
    if (context) {
        context->verbose_logging = verbose;
    }
}

bool phantom_get_verbose(Program* program) {
    PhantomIDContext* context = program->user_data;
    return context ? context->verbose_logging : false;
}

// Program registration helper
bool phantom_register(void) {
    return program_register(&phantom_interface);
}

// Validation helpers
static bool validate_message(const Message* msg) {
    if (!msg) return false;
    
    // Basic validation
    if (!msg->source[0]) return false;
    if (msg->data_size > 0 && !msg->data) return false;
    if (msg->timestamp > time(NULL)) return false;

    // Type-specific validation
    switch (msg->type) {
        case MSG_NODE_CREATED:
        case MSG_NODE_DELETED:
            if (!msg->data || msg->data_size != sizeof(char[64])) return false;
            break;

        case MSG_DATA:
            if (!msg->target[0]) return false;
            break;

        case MSG_NETWORK:
            if (msg->data_size != sizeof(NetworkState)) return false;
            break;

        default:
            return false;
    }

    return true;
}

static bool parse_command(const char* cmd_str, Command* cmd) {
    if (!cmd_str || !cmd) return false;

    // Parse command string
    char type[32] = {0};
    char source[64] = {0};
    
    if (sscanf(cmd_str, "%31s %63s", type, source) != 2) {
        return false;
    }

    // Set command type
    if (strcmp(type, "node") == 0) {
        cmd->type = CMD_NODE;
    } else if (strcmp(type, "program") == 0) {
        cmd->type = CMD_PROGRAM;
    } else {
        return false;
    }

    // Set source and parse remaining data
    strncpy(cmd->source, source, sizeof(cmd->source) - 1);
    
    const char* data_start = strchr(cmd_str + strlen(type) + strlen(source) + 2, ' ');
    if (data_start) {
        cmd->data = (void*)(data_start + 1);
        cmd->data_size = strlen(data_start + 1);
    }

    return true;
}