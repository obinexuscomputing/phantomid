#ifndef CLI_H
#define CLI_H

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

#include "../tree/tree.h"
#include "../network/network.h"
#include "../state/state.h"

// Command result codes
typedef enum {
    CMD_SUCCESS = 0,
    CMD_ERROR_INVALID = -1,
    CMD_ERROR_ARGS = -2,
    CMD_ERROR_EXEC = -3,
    CMD_ERROR_STATE = -4,
    CMD_EXIT = 1
} CommandResult;

// CLI context structure
typedef struct {
    TreeContext* tree;          // Tree management context
    NetworkContext* network;    // Network management context
    StateContext* state;        // State management context
    bool verbose;              // Verbose output flag
    bool running;              // CLI running state
    char last_error[256];      // Last error message
} CLIContext;

// CLI operations
CLIContext* cli_create(TreeContext* tree, NetworkContext* network, StateContext* state);
void cli_destroy(CLIContext* ctx);

// Command processing
CommandResult cli_process_command(CLIContext* ctx, const char* command);
const char* cli_get_error(const CLIContext* ctx);

// Command execution
CommandResult cli_execute_node(CLIContext* ctx, int argc, char** argv);
CommandResult cli_execute_network(CLIContext* ctx, int argc, char** argv);
CommandResult cli_execute_state(CLIContext* ctx, int argc, char** argv);

// Output formatting
void cli_print_status(const CLIContext* ctx);
void cli_print_help(void);
void cli_print_version(void);

#endif // CLI_H