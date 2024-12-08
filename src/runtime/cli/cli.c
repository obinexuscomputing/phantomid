#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cli.h"

#define VERSION "1.0.0"
#define MAX_ARGS 16
#define MAX_LINE 1024

// Command parsing helpers
static void parse_args(char* line, int* argc, char** argv) {
    *argc = 0;
    char* token = strtok(line, " \t\n");
    
    while (token && *argc < MAX_ARGS) {
        argv[(*argc)++] = token;
        token = strtok(NULL, " \t\n");
    }
}

// Error handling
static void set_error(CLIContext* ctx, const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(ctx->last_error, sizeof(ctx->last_error), format, args);
    va_end(args);
}

// Create CLI context
CLIContext* cli_create(TreeContext* tree, NetworkContext* network, StateContext* state) {
    CLIContext* ctx = calloc(1, sizeof(CLIContext));
    if (!ctx) return NULL;

    ctx->tree = tree;
    ctx->network = network;
    ctx->state = state;
    ctx->running = true;
    
    return ctx;
}

// Process node commands
CommandResult cli_execute_node(CLIContext* ctx, int argc, char** argv) {
    if (argc < 2) {
        set_error(ctx, "Missing node command. Usage: node <create|delete|list> [args]");
        return CMD_ERROR_ARGS;
    }

    if (strcmp(argv[1], "create") == 0) {
        const char* parent_id = argc > 2 ? argv[2] : NULL;
        TreeNode* node = tree_create_node(ctx->tree, parent_id);
        
        if (node) {
            printf("Created node: %s\n", node->id);
            if (parent_id) {
                printf("Parent: %s\n", parent_id);
            }
            return CMD_SUCCESS;
        }
        
        set_error(ctx, "Failed to create node");
        return CMD_ERROR_EXEC;
    }
    
    else if (strcmp(argv[1], "delete") == 0) {
        if (argc < 3) {
            set_error(ctx, "Missing node ID. Usage: node delete <id>");
            return CMD_ERROR_ARGS;
        }
        
        if (tree_delete_node(ctx->tree, argv[2])) {
            printf("Deleted node: %s\n", argv[2]);
            return CMD_SUCCESS;
        }
        
        set_error(ctx, "Failed to delete node");
        return CMD_ERROR_EXEC;
    }
    
    else if (strcmp(argv[1], "list") == 0) {
        printf("\nNode List:\n");
        tree_traverse_dfs(ctx->tree, print_node, NULL);
        return CMD_SUCCESS;
    }

    set_error(ctx, "Unknown node command: %s", argv[1]);
    return CMD_ERROR_INVALID;
}

// Process network commands
CommandResult cli_execute_network(CLIContext* ctx, int argc, char** argv) {
    if (argc < 2) {
        set_error(ctx, "Missing network command. Usage: net <status|send|broadcast> [args]");
        return CMD_ERROR_ARGS;
    }

    if (strcmp(argv[1], "status") == 0) {
        printf("\nNetwork Status:\n");
        printf("Active Connections: %zu/%zu\n", 
               ctx->network->active_connections,
               ctx->network->max_connections);
        printf("Port: %u\n", ctx->network->port);
        return CMD_SUCCESS;
    }
    
    else if (strcmp(argv[1], "send") == 0) {
        if (argc < 4) {
            set_error(ctx, "Usage: net send <node_id> <message>");
            return CMD_ERROR_ARGS;
        }
        
        NetworkMessage msg = {
            .type = MSG_DATA,
            .data_size = strlen(argv[3])
        };
        memcpy(msg.data, argv[3], msg.data_size);
        
        if (network_send(ctx->network, argv[2], &msg)) {
            printf("Message sent to %s\n", argv[2]);
            return CMD_SUCCESS;
        }
        
        set_error(ctx, "Failed to send message");
        return CMD_ERROR_EXEC;
    }
    
    else if (strcmp(argv[1], "broadcast") == 0) {
        if (argc < 3) {
            set_error(ctx, "Usage: net broadcast <message>");
            return CMD_ERROR_ARGS;
        }
        
        NetworkMessage msg = {
            .type = MSG_DATA,
            .data_size = strlen(argv[2])
        };
        memcpy(msg.data, argv[2], msg.data_size);
        
        if (network_broadcast(ctx->network, &msg)) {
            printf("Message broadcast to all nodes\n");
            return CMD_SUCCESS;
        }
        
        set_error(ctx, "Failed to broadcast message");
        return CMD_ERROR_EXEC;
    }

    set_error(ctx, "Unknown network command: %s", argv[1]);
    return CMD_ERROR_INVALID;
}

// Process state commands
CommandResult cli_execute_state(CLIContext* ctx, int argc, char** argv) {
    if (argc < 2) {
        set_error(ctx, "Missing state command. Usage: state <save|load|info>");
        return CMD_ERROR_ARGS;
    }

    if (strcmp(argv[1], "save") == 0) {
        if (state_save(ctx->state, ctx->tree)) {
            printf("State saved successfully\n");
            return CMD_SUCCESS;
        }
        
        set_error(ctx, "Failed to save state");
        return CMD_ERROR_STATE;
    }
    
    else if (strcmp(argv[1], "load") == 0) {
        if (state_load(ctx->state, ctx->tree)) {
            printf("State loaded successfully\n");
            return CMD_SUCCESS;
        }
        
        set_error(ctx, "Failed to load state");
        return CMD_ERROR_STATE;
    }
    
    else if (strcmp(argv[1], "info") == 0) {
        printf("\nState Information:\n");
        printf("Version: %u\n", state_get_version(ctx->state));
        printf("Nodes: %zu\n", state_get_node_count(ctx->state));
        printf("Last Save: %s", ctime(&ctx->state->header.timestamp));
        printf("Checksum Valid: %s\n", 
               state_verify_checksum(ctx->state) ? "Yes" : "No");
        return CMD_SUCCESS;
    }

    set_error(ctx, "Unknown state command: %s", argv[1]);
    return CMD_ERROR_INVALID;
}

// Process commands
CommandResult cli_process_command(CLIContext* ctx, const char* command) {
    if (!ctx || !command) return CMD_ERROR_INVALID;

    // Parse command line
    char line[MAX_LINE];
    strncpy(line, command, sizeof(line) - 1);
    
    int argc = 0;
    char* argv[MAX_ARGS];
    parse_args(line, &argc, argv);
    
    if (argc == 0) return CMD_SUCCESS;

    // Process commands
    if (strcmp(argv[0], "help") == 0) {
        cli_print_help();
        return CMD_SUCCESS;
    }
    
    else if (strcmp(argv[0], "version") == 0) {
        cli_print_version();
        return CMD_SUCCESS;
    }
    
    else if (strcmp(argv[0], "status") == 0) {
        cli_print_status(ctx);
        return CMD_SUCCESS;
    }
    
    else if (strcmp(argv[0], "exit") == 0 || 
             strcmp(argv[0], "quit") == 0) {
        ctx->running = false;
        return CMD_EXIT;
    }
    
    else if (strcmp(argv[0], "node") == 0) {
        return cli_execute_node(ctx, argc, argv);
    }
    
    else if (strcmp(argv[0], "net") == 0) {
        return cli_execute_network(ctx, argc, argv);
    }
    
    else if (strcmp(argv[0], "state") == 0) {
        return cli_execute_state(ctx, argc, argv);
    }

    set_error(ctx, "Unknown command: %s", argv[0]);
    return CMD_ERROR_INVALID;
}

// Output formatting
void cli_print_status(const CLIContext* ctx) {
    printf("\nPhantomID Status\n");
    printf("---------------\n");
    printf("Nodes: %zu\n", tree_get_size(ctx->tree));
    printf("Tree Depth: %zu\n", tree_get_depth(ctx->tree));
    printf("Root Node: %s\n", tree_has_root(ctx->tree) ? "Present" : "None");
    printf("Active Connections: %zu/%zu\n",
           ctx->network->active_connections,
           ctx->network->max_connections);
    printf("State Version: %u\n", state_get_version(ctx->state));
}

void cli_print_help(void) {
    printf("\nPhantomID Commands:\n");
    printf("  node create [parent_id]    Create new node\n");
    printf("  node delete <id>           Delete node\n");
    printf("  node list                  List all nodes\n");
    printf("\n");
    printf("  net status                 Show network status\n");
    printf("  net send <id> <msg>        Send message to node\n");
    printf("  net broadcast <msg>        Broadcast message\n");
    printf("\n");
    printf("  state save                 Save current state\n");
    printf("  state load                 Load saved state\n");
    printf("  state info                 Show state info\n");
    printf("\n");
    printf("  status                     Show system status\n");
    printf("  help                       Show this help\n");
    printf("  version                    Show version\n");
    printf("  exit                       Exit program\n");
}

void cli_print_version(void) {
    printf("PhantomID version %s\n", VERSION);
}

const char* cli_get_error(const CLIContext* ctx) {
    return ctx ? ctx->last_error : "Invalid context";
}

void cli_destroy(CLIContext* ctx) {
    free(ctx);
}