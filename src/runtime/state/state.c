#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "state.h"

#define STATE_MAGIC 0x50484944 // "PHID"
#define STATE_BUFFER_SIZE 4096

// Calculate checksum of data
static uint32_t calculate_checksum(const void* data, size_t size) {
    const uint8_t* bytes = data;
    uint32_t checksum = 0;
    
    for (size_t i = 0; i < size; i++) {
        checksum = ((checksum << 5) + checksum) + bytes[i];
    }
    
    return checksum;
}

// Create state context
StateContext* state_create(const char* filename) {
    if (!filename) return NULL;

    StateContext* ctx = calloc(1, sizeof(StateContext));
    if (!ctx) return NULL;

    ctx->filename = strdup(filename);
    if (!ctx->filename) {
        free(ctx);
        return NULL;
    }

    if (pthread_mutex_init(&ctx->lock, NULL) != 0) {
        free(ctx->filename);
        free(ctx);
        return NULL;
    }

    // Initialize header
    ctx->header.magic = STATE_MAGIC;
    ctx->header.version = STATE_VERSION;
    ctx->header.flags = STATE_FLAG_NONE;
    ctx->header.timestamp = time(NULL);
    ctx->header.node_count = 0;
    ctx->header.checksum = 0;

    return ctx;
}

// Node visitor for counting nodes
static void count_nodes(TreeNode* node, void* user_data) {
    size_t* count = user_data;
    (*count)++;
}

// Node visitor for saving nodes
static void save_node(TreeNode* node, void* user_data) {
    FILE* file = user_data;
    if (!file || !node) return;

    NodeState state = {0};
    strncpy(state.id, node->id, sizeof(state.id) - 1);
    if (node->parent) {
        strncpy(state.parent_id, node->parent->id, sizeof(state.parent_id) - 1);
    }
    state.creation_time = node->creation_time;
    state.is_root = node->is_root;
    state.is_active = node->is_active;
    state.child_count = node->child_count;

    fwrite(&state, sizeof(NodeState), 1, file);
}

// Save state to file
bool state_save(StateContext* ctx, TreeContext* tree) {
    if (!ctx || !tree) return false;

    pthread_mutex_lock(&ctx->lock);

    FILE* file = fopen(ctx->filename, "wb");
    if (!file) {
        pthread_mutex_unlock(&ctx->lock);
        return false;
    }

    // Count nodes first
    size_t node_count = 0;
    tree_traverse_dfs(tree, count_nodes, &node_count);
    ctx->header.node_count = node_count;
    ctx->header.timestamp = time(NULL);

    // Write header placeholder
    fwrite(&ctx->header, sizeof(StateHeader), 1, file);

    // Write nodes
    tree_traverse_dfs(tree, save_node, file);

    // Calculate and update checksum
    long current_pos = ftell(file);
    fseek(file, sizeof(StateHeader), SEEK_SET);
    
    uint8_t* buffer = malloc(STATE_BUFFER_SIZE);
    if (buffer) {
        size_t bytes;
        uint32_t checksum = 0;
        
        while ((bytes = fread(buffer, 1, STATE_BUFFER_SIZE, file)) > 0) {
            checksum ^= calculate_checksum(buffer, bytes);
        }
        
        free(buffer);
        
        // Update header with checksum
        ctx->header.checksum = checksum;
        fseek(file, 0, SEEK_SET);
        fwrite(&ctx->header, sizeof(StateHeader), 1, file);
    }

    fclose(file);
    pthread_mutex_unlock(&ctx->lock);
    return true;
}

// Load state from file
bool state_load(StateContext* ctx, TreeContext* tree) {
    if (!ctx || !tree) return false;

    pthread_mutex_lock(&ctx->lock);

    FILE* file = fopen(ctx->filename, "rb");
    if (!file) {
        pthread_mutex_unlock(&ctx->lock);
        return false;
    }

    // Read and verify header
    StateHeader header;
    if (fread(&header, sizeof(StateHeader), 1, file) != 1 ||
        header.magic != STATE_MAGIC ||
        header.version > STATE_VERSION) {
        fclose(file);
        pthread_mutex_unlock(&ctx->lock);
        return false;
    }

    // Store header
    memcpy(&ctx->header, &header, sizeof(StateHeader));

    // Read nodes
    NodeState state;
    for (size_t i = 0; i < header.node_count; i++) {
        if (fread(&state, sizeof(NodeState), 1, file) != 1) {
            fclose(file);
            pthread_mutex_unlock(&ctx->lock);
            return false;
        }

        // Create node
        TreeNode* node = tree_create_node(tree, state.parent_id);
        if (node) {
            strncpy(node->id, state.id, sizeof(node->id) - 1);
            node->creation_time = state.creation_time;
            node->is_root = state.is_root;
            node->is_active = state.is_active;
        }
    }

    fclose(file);
    pthread_mutex_unlock(&ctx->lock);
    return true;
}

// Version management
uint32_t state_get_version(StateContext* ctx) {
    return ctx ? ctx->header.version : 0;
}

bool state_is_compatible(StateContext* ctx) {
    return ctx && ctx->header.version <= STATE_VERSION;
}

// State information
time_t state_get_timestamp(StateContext* ctx) {
    return ctx ? ctx->header.timestamp : 0;
}

size_t state_get_node_count(StateContext* ctx) {
    return ctx ? ctx->header.node_count : 0;
}

bool state_verify_checksum(StateContext* ctx) {
    if (!ctx) return false;

    FILE* file = fopen(ctx->filename, "rb");
    if (!file) return false;

    // Skip header
    fseek(file, sizeof(StateHeader), SEEK_SET);

    // Calculate checksum
    uint8_t* buffer = malloc(STATE_BUFFER_SIZE);
    if (!buffer) {
        fclose(file);
        return false;
    }

    size_t bytes;
    uint32_t checksum = 0;
    
    while ((bytes = fread(buffer, 1, STATE_BUFFER_SIZE, file)) > 0) {
        checksum ^= calculate_checksum(buffer, bytes);
    }
    
    free(buffer);
    fclose(file);

    return checksum == ctx->header.checksum;
}

// Feature flags
void state_set_compression(StateContext* ctx, bool enabled) {
    if (!ctx) return;
    if (enabled) {
        ctx->flags |= STATE_FLAG_COMPRESSED;
    } else {
        ctx->flags &= ~STATE_FLAG_COMPRESSED;
    }
}

void state_set_encryption(StateContext* ctx, bool enabled) {
    if (!ctx) return;
    if (enabled) {
        ctx->flags |= STATE_FLAG_ENCRYPTED;
    } else {
        ctx->flags &= ~STATE_FLAG_ENCRYPTED;
    }
}

bool state_is_compressed(StateContext* ctx) {
    return ctx && (ctx->flags & STATE_FLAG_COMPRESSED);
}

bool state_is_encrypted(StateContext* ctx) {
    return ctx && (ctx->flags & STATE_FLAG_ENCRYPTED);
}

// Cleanup
void state_destroy(StateContext* ctx) {
    if (!ctx) return;

    pthread_mutex_lock(&ctx->lock);
    free(ctx->filename);
    pthread_mutex_unlock(&ctx->lock);
    
    pthread_mutex_destroy(&ctx->lock);
    free(ctx);
}