#ifndef STATE_H
#define STATE_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "../tree/tree.h"

// State file version
#define STATE_VERSION 1

// State flags
typedef enum {
    STATE_FLAG_NONE = 0,
    STATE_FLAG_COMPRESSED = 1,
    STATE_FLAG_ENCRYPTED = 2
} StateFlags;

// State header structure
typedef struct {
    uint32_t magic;          // Magic number to identify state file
    uint32_t version;        // State format version
    uint32_t flags;          // State flags (compression, encryption, etc)
    time_t timestamp;        // When state was saved
    size_t node_count;       // Number of nodes in state
    uint32_t checksum;       // State data checksum
} StateHeader;

// Node state structure
typedef struct {
    char id[64];            // Node identifier
    char parent_id[64];     // Parent node ID
    time_t creation_time;   // When node was created
    bool is_root;           // If node is root
    bool is_active;         // If node is active
    size_t child_count;     // Number of children
} NodeState;

// State context structure
typedef struct {
    char* filename;         // State file path
    StateFlags flags;       // Current state flags
    StateHeader header;     // Current state header
    pthread_mutex_t lock;   // Thread safety lock
} StateContext;

// Basic state operations
StateContext* state_create(const char* filename);
void state_destroy(StateContext* ctx);

// State file operations
bool state_save(StateContext* ctx, TreeContext* tree);
bool state_load(StateContext* ctx, TreeContext* tree);

// Version management
uint32_t state_get_version(StateContext* ctx);
bool state_is_compatible(StateContext* ctx);

// State information
time_t state_get_timestamp(StateContext* ctx);
size_t state_get_node_count(StateContext* ctx);
bool state_verify_checksum(StateContext* ctx);

// Feature flags
void state_set_compression(StateContext* ctx, bool enabled);
void state_set_encryption(StateContext* ctx, bool enabled);
bool state_is_compressed(StateContext* ctx);
bool state_is_encrypted(StateContext* ctx);

#endif // STATE_H