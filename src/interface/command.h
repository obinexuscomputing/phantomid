#ifndef COMMAND_INTERFACE_H
#define COMMAND_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>
#include "program.h"

// Command types
typedef enum {
    CMD_PROGRAM = 0,    // Program control commands
    CMD_NODE,           // Node management
    CMD_NETWORK,        // Network operations
    CMD_STATE,          // State management
    CMD_ADMIN,          // Administrative tasks
    CMD_CUSTOM = 1000   // Base for program-specific commands
} CommandType;

// Command status
typedef enum {
    CMD_STATUS_SUCCESS = 0,
    CMD_STATUS_ERROR = -1,
    CMD_STATUS_INVALID = -2,
    CMD_STATUS_DENIED = -3
} CommandStatus;

// Command structure
typedef struct {
    CommandType type;           // Command type
    uint32_t id;               // Command identifier
    char source[64];           // Command source
    char target[64];           // Command target
    void* data;                // Command data
    size_t data_size;         // Data size
} Command;

// Command response
typedef struct {
    uint32_t command_id;       // Original command ID
    CommandStatus status;      // Command result
    void* data;                // Response data
    size_t data_size;         // Response size
} CommandResponse;

// Command handler callback
typedef CommandStatus (*CommandHandlerFn)(Program* program, 
                                        const Command* command,
                                        CommandResponse* response);

// Command interface
typedef struct {
    // Command registration
    bool (*register_handler)(Program* program, 
                           CommandType type,
                           CommandHandlerFn handler);
                           
    // Command execution
    CommandStatus (*execute)(Program* program,
                           const Command* command,
                           CommandResponse* response);
                           
    // Command routing
    bool (*route)(Program* program,
                 const char* target,
                 const Command* command);
    
    // Command validation
    bool (*validate)(Program* program,
                    const Command* command);
} CommandInterface;

#endif // COMMAND_INTERFACE_H