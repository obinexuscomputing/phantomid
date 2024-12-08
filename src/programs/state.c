#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../interface/program.h"
#include "../interface/state.h"
#include "../runtime/tree/tree.h"
#include "phantomid.h"

// Program state types
typedef enum {
    PHANTOM_STATE_NODE = STATE_CUSTOM,
    PHANTOM_STATE_NETWORK,
    PHANTOM_STATE_CONFIG
} PhantomStateType;

// Node state structure
typedef struct {
    char id[64];
    char parent_id[64];
    time_t creation_time;
    bool is_root;
    bool is_admin;
    size_t child_count;
    size_t max_children;
} NodeState;

// Network state structure
typedef struct {
    uint16_t port;
    size_t max_connections;
    time_t last_activity;
    size_t active_connections;
} NetworkState;

// Configuration state structure
typedef struct {
    bool verbose_logging;
    bool auto_save;
    uint32_t save_interval;
    char state_dir[256];
} ConfigState;

// Program state context
typedef struct {
    StateInterface* state;
    TreeContext* tree;
    time_t last_save;
    ConfigState config;
} ProgramState;

// Save node state
static bool save_node_state(Program* program, TreeNode* node, StateEntry* entry) {
    NodeState state = {
        .creation_time = node->creation_time,
        .is_root = node->is_root,
        .is_admin = node->is_admin,
        .child_count = node->child_count,
        .max_children = node->max_children
    };
    
    strncpy(state.id, node->id, sizeof(state.id) - 1);
    if (node->parent) {
        strncpy(state.parent_id, node->parent->id, sizeof(state.parent_id) - 1);
    }

    entry->type = PHANTOM_STATE_NODE;
    entry->data = malloc(sizeof(NodeState));
    if (!entry->data) return false;

    memcpy(entry->data, &state, sizeof(NodeState));
    entry->data_size = sizeof(NodeState);
    strncpy(entry->id, node->id, sizeof(entry->id) - 1);

    return program->state->set_entry(program, entry);
}

// Load node state
static bool load_node_state(Program* program, const StateEntry* entry) {
    if (entry->type != PHANTOM_STATE_NODE || !entry->data) return false;

    NodeState* state = entry->data;
    ProgramState* prog_state = program->user_data;

    // Create node in tree
    TreeNode* node = tree_create_node(prog_state->tree, state->parent_id);
    if (!node) return false;

    strncpy(node->id, state->id, sizeof(node->id) - 1);
    node->creation_time = state->creation_time;
    node->is_root = state->is_root;
    node->is_admin = state->is_admin;
    node->max_children = state->max_children;

    return true;
}

// Save network state
static bool save_network_state(Program* program) {
    NetworkState state = {
        .port = program->interface->requirements.default_port,
        .max_connections = program->interface->requirements.max_connections,
        .last_activity = time(NULL),
        .active_connections = 0  // Would be updated from network context
    };

    StateEntry entry = {
        .type = PHANTOM_STATE_NETWORK,
        .data = &state,
        .data_size = sizeof(NetworkState)
    };
    strcpy(entry.id, "network");

    return program->state->set_entry(program, &entry);
}

// Load network state
static bool load_network_state(Program* program, const StateEntry* entry) {
    if (entry->type != PHANTOM_STATE_NETWORK || !entry->data) return false;

    NetworkState* state = entry->data;
    // Would update network configuration here
    // For now just verify the data
    
    return state->port == program->interface->requirements.default_port;
}

// Save configuration state
static bool save_config_state(Program* program) {
    ProgramState* state = program->user_data;
    
    StateEntry entry = {
        .type = PHANTOM_STATE_CONFIG,
        .data = &state->config,
        .data_size = sizeof(ConfigState)
    };
    strcpy(entry.id, "config");

    return program->state->set_entry(program, &entry);
}

// Load configuration state
static bool load_config_state(Program* program, const StateEntry* entry) {
    if (entry->type != PHANTOM_STATE_CONFIG || !entry->data) return false;

    ProgramState* state = program->user_data;
    memcpy(&state->config, entry->data, sizeof(ConfigState));

    return true;
}

// Node visitor for saving state
static void save_node_visitor(TreeNode* node, void* user_data) {
    Program* program = user_data;
    StateEntry entry = {0};
    save_node_state(program, node, &entry);
}

// Save all state
bool phantom_save_state(Program* program) {
    ProgramState* state = program->user_data;
    state->last_save = time(NULL);

    // Save nodes
    tree_traverse_dfs(state->tree, save_node_visitor, program);

    // Save network state
    save_network_state(program);

    // Save configuration
    save_config_state(program);

    return true;
}

// State change handler
static void handle_state_change(Program* program, StateType type, const char* id) {
    StateEntry entry;
    
    if (!program->state->get_entry(program, type, id, &entry)) {
        return;
    }

    switch (type) {
        case PHANTOM_STATE_NODE:
            load_node_state(program, &entry);
            break;
            
        case PHANTOM_STATE_NETWORK:
            load_network_state(program, &entry);
            break;
            
        case PHANTOM_STATE_CONFIG:
            load_config_state(program, &entry);
            break;

        default:
            break;
    }

    if (entry.data) {
        free(entry.data);
    }
}

// Initialize program state
bool phantom_state_init(Program* program) {
    ProgramState* state = calloc(1, sizeof(ProgramState));
    if (!state) return false;

    // Get interfaces
    state->state = program_get_state(program);
    if (!state->state) {
        free(state);
        return false;
    }

    // Set default config
    state->config.verbose_logging = false;
    state->config.auto_save = true;
    state->config.save_interval = 300; // 5 minutes
    strcpy(state->config.state_dir, "state");

    // Register state change handlers
    state->state->register_handler(program, PHANTOM_STATE_NODE, handle_state_change);
    state->state->register_handler(program, PHANTOM_STATE_NETWORK, handle_state_change);
    state->state->register_handler(program, PHANTOM_STATE_CONFIG, handle_state_change);

    program->user_data = state;
    return true;
}

// Cleanup program state
void phantom_state_cleanup(Program* program) {
    if (!program) return;

    ProgramState* state = program->user_data;
    if (state) {
        if (state->config.auto_save) {
            phantom_save_state(program);
        }
        free(state);
    }
}

// Check if state save needed
bool phantom_check_state(Program* program) {
    ProgramState* state = program->user_data;
    if (!state || !state->config.auto_save) return false;

    time_t now = time(NULL);
    if (now - state->last_save >= state->config.save_interval) {
        return phantom_save_state(program);
    }

    return true;
}