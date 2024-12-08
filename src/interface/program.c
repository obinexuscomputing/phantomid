#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "program.h"
#include "message.h"
#include "command.h"
#include "state.h"

// Maximum number of registered program interfaces
#define MAX_PROGRAMS 32

// Registry for program interfaces
static struct {
    const ProgramInterface* interfaces[MAX_PROGRAMS];
    size_t count;
} program_registry;

// Program registration
bool program_register(const ProgramInterface* interface) {
    if (!interface || program_registry.count >= MAX_PROGRAMS) {
        return false;
    }

    // Check for duplicate registration
    for (size_t i = 0; i < program_registry.count; i++) {
        if (strcmp(program_registry.interfaces[i]->name, interface->name) == 0) {
            return false;
        }
    }

    program_registry.interfaces[program_registry.count++] = interface;
    return true;
}

// Find registered program interface
static const ProgramInterface* find_interface(const char* name) {
    for (size_t i = 0; i < program_registry.count; i++) {
        if (strcmp(program_registry.interfaces[i]->name, name) == 0) {
            return program_registry.interfaces[i];
        }
    }
    return NULL;
}

// Initialize program runtime
static bool init_runtime(Program* program) {
    if (!program || !program->interface) return false;

    const ProgramInterface* interface = program->interface;
    ProgramRuntime* runtime = &program->runtime;

    // Initialize network if required
    if (interface->requirements.needs_network) {
        runtime->network_context = network_create(interface->requirements.default_port);
        if (!runtime->network_context) return false;
    }

    // Initialize state if required
    if (interface->requirements.needs_persistence) {
        static char state_file[256];
        snprintf(state_file, sizeof(state_file), "%s.state", interface->name);
        runtime->state_context = state_create(state_file);
        if (!runtime->state_context) {
            if (runtime->network_context) {
                network_destroy(runtime->network_context);
            }
            return false;
        }
    }

    // Initialize CLI if required
    if (interface->requirements.needs_cli) {
        runtime->cli_context = cli_create(runtime->network_context, runtime->state_context);
        if (!runtime->cli_context) {
            if (runtime->network_context) {
                network_destroy(runtime->network_context);
            }
            if (runtime->state_context) {
                state_destroy(runtime->state_context);
            }
            return false;
        }
    }

    runtime->is_running = true;
    return true;
}

// Cleanup program runtime
static void cleanup_runtime(Program* program) {
    if (!program) return;

    ProgramRuntime* runtime = &program->runtime;

    // Cleanup CLI
    if (runtime->cli_context) {
        cli_destroy(runtime->cli_context);
        runtime->cli_context = NULL;
    }

    // Cleanup state
    if (runtime->state_context) {
        state_destroy(runtime->state_context);
        runtime->state_context = NULL;
    }

    // Cleanup network
    if (runtime->network_context) {
        network_destroy(runtime->network_context);
        runtime->network_context = NULL;
    }

    runtime->is_running = false;
}

// Create program instance
Program* program_create(const char* name) {
    if (!name) return NULL;

    const ProgramInterface* interface = find_interface(name);
    if (!interface) return NULL;

    Program* program = calloc(1, sizeof(Program));
    if (!program) return NULL;

    program->interface = interface;
    
    // Initialize runtime
    if (!init_runtime(program)) {
        free(program);
        return NULL;
    }

    // Initialize program
    if (interface->init && !interface->init(program)) {
        cleanup_runtime(program);
        free(program);
        return NULL;
    }

    return program;
}

// Start program
bool program_start(Program* program) {
    if (!program || !program->interface) return false;

    ProgramRuntime* runtime = &program->runtime;

    // Start network if present
    if (runtime->network_context) {
        if (!network_start(runtime->network_context)) {
            return false;
        }
    }

    // Load state if present
    if (runtime->state_context) {
        state_load(runtime->state_context, NULL); // Ignore load failures
    }

    return true;
}

// Stop program
void program_stop(Program* program) {
    if (!program || !program->interface) return;

    ProgramRuntime* runtime = &program->runtime;
    runtime->is_running = false;

    // Save state if present
    if (runtime->state_context) {
        state_save(runtime->state_context, NULL);
    }

    // Stop network if present
    if (runtime->network_context) {
        network_stop(runtime->network_context);
    }
}

// Destroy program
void program_destroy(Program* program) {
    if (!program) return;

    // Call program cleanup
    if (program->interface && program->interface->cleanup) {
        program->interface->cleanup(program);
    }

    // Cleanup runtime
    cleanup_runtime(program);

    // Free program structure
    free(program);
}

// Runtime accessors
void* program_get_network(Program* program) {
    return program ? program->runtime.network_context : NULL;
}

void* program_get_state(Program* program) {
    return program ? program->runtime.state_context : NULL;
}

void* program_get_cli(Program* program) {
    return program ? program->runtime.cli_context : NULL;
}

// Program management
static bool dispatch_message(Program* program, const void* message, size_t size) {
    if (!program || !program->interface || !program->interface->handle_message) {
        return false;
    }
    return program->interface->handle_message(program, message, size);
}

static bool dispatch_command(Program* program, const char* command, void* response) {
    if (!program || !program->interface || !program->interface->handle_command) {
        return false;
    }
    return program->interface->handle_command(program, command, response);
}

// Main program run loop
void program_run(Program* program) {
    if (!program || !program->interface) return;

    ProgramRuntime* runtime = &program->runtime;

    // Call program's run method if provided
    if (program->interface->run) {
        program->interface->run(program);
    }

    // Run network processing if present
    if (runtime->network_context) {
        network_run(runtime->network_context);
    }

    // Run CLI processing if present
    if (runtime->cli_context) {
        // CLI processing would go here
    }
}