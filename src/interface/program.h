#ifndef PROGRAM_INTERFACE_H
#define PROGRAM_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>

// Forward declarations
typedef struct Program Program;
typedef struct ProgramRuntime ProgramRuntime;
typedef struct ProgramInterface ProgramInterface;

// Program lifecycle callbacks
typedef bool (*ProgramInitFn)(Program* program);
typedef void (*ProgramCleanupFn)(Program* program);
typedef void (*ProgramRunFn)(Program* program);

// Program message handling
typedef bool (*ProgramMessageHandlerFn)(Program* program, const void* message, size_t size);
typedef bool (*ProgramCommandHandlerFn)(Program* program, const char* command, void* response);

// Program interface definition
struct ProgramInterface {
    // Program identification
    const char* name;
    const char* version;
    uint32_t interface_version;
    
    // Lifecycle methods
    ProgramInitFn init;
    ProgramCleanupFn cleanup;
    ProgramRunFn run;
    
    // Message handling
    ProgramMessageHandlerFn handle_message;
    ProgramCommandHandlerFn handle_command;
    
    // Runtime requirements
    struct {
        bool needs_network;
        bool needs_persistence;
        bool needs_cli;
        uint16_t default_port;
        uint32_t max_connections;
    } requirements;
};

// Program runtime state
struct ProgramRuntime {
    void* network_context;
    void* state_context;
    void* cli_context;
    bool is_running;
};

// Complete program structure
struct Program {
    const ProgramInterface* interface;
    ProgramRuntime runtime;
    void* user_data;
};

// Program registration and management
bool program_register(const ProgramInterface* interface);
Program* program_create(const char* name);
void program_destroy(Program* program);
bool program_start(Program* program);
void program_stop(Program* program);

// Runtime accessors
void* program_get_network(Program* program);
void* program_get_state(Program* program);
void* program_get_cli(Program* program);

#endif // PROGRAM_INTERFACE_H