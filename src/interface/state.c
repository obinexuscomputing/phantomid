#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include "state.h"

#define STATE_BUFFER_SIZE 4096
#define MAX_HANDLERS 32
#define MAGIC_NUMBER 0x50484944 // "PHID"

// State change handler registry
typedef struct {
    StateType type;
    StateChangeHandlerFn handler;
} StateHandler;

// State context stored in program user_data
typedef struct {
    StateHeader header;
    StateHandler handlers[MAX_HANDLERS];
    size_t handler_count;
    char* filename;
    pthread_mutex_t lock;
    uint32_t checksum;
    void* cache;
    size_t cache_size;
} StateContext;

// Calculate checksum for data
static uint32_t calculate_checksum(const void* data, size_t size) {
    const uint8_t* bytes = data;
    uint32_t checksum = 0;
    
    for (size_t i = 0; i < size; i++) {
        checksum = ((checksum << 5) + checksum) + bytes[i];
    }
    
    return checksum;
}

// Initialize state context
static bool init_state_context(StateContext* ctx, const char* filename) {
    if (!ctx || !filename) return false;
    
    ctx->filename = strdup(filename);
    if (!ctx->filename) return false;
    
    ctx->header.magic = MAGIC_NUMBER;
    ctx->header.version = 1;
    ctx->header.flags = STATE_FLAG_NONE;
    ctx->header.timestamp = time(NULL);
    ctx->header.checksum = 0;
    ctx->handler_count = 0;
    
    if (pthread_mutex_init(&ctx->lock, NULL) != 0) {
        free(ctx->filename);
        return false;
    }
    
    return true;
}

// Save state to file
static bool save_state(Program* program, const char* filename) {
    if (!program || !filename) return false;
    
    StateContext* ctx = program->user_data;
    if (!ctx) return false;
    
    pthread_mutex_lock(&ctx->lock);
    
    FILE* file = fopen(filename, "wb");
    if (!file) {
        pthread_mutex_unlock(&ctx->lock);
        return false;
    }
    
    // Update header
    ctx->header.timestamp = time(NULL);
    ctx->header.checksum = 0;  // Will update after writing data
    
    // Write header
    if (fwrite(&ctx->header, sizeof(StateHeader), 1, file) != 1) {
        fclose(file);
        pthread_mutex_unlock(&ctx->lock);
        return false;
    }
    
    // Write cached state data
    if (ctx->cache && ctx->cache_size > 0) {
        if (fwrite(ctx->cache, ctx->cache_size, 1, file) != 1) {
            fclose(file);
            pthread_mutex_unlock(&ctx->lock);
            return false;
        }
    }
    
    // Calculate checksum
    fseek(file, sizeof(StateHeader), SEEK_SET);
    uint8_t buffer[STATE_BUFFER_SIZE];
    size_t bytes_read;
    uint32_t checksum = 0;
    
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        checksum ^= calculate_checksum(buffer, bytes_read);
    }
    
    // Update and write header with checksum
    ctx->header.checksum = checksum;
    fseek(file, 0, SEEK_SET);
    fwrite(&ctx->header, sizeof(StateHeader), 1, file);
    
    fclose(file);
    pthread_mutex_unlock(&ctx->lock);
    return true;
}

// Load state from file
static bool load_state(Program* program, const char* filename) {
    if (!program || !filename) return false;
    
    StateContext* ctx = program->user_data;
    if (!ctx) return false;
    
    pthread_mutex_lock(&ctx->lock);
    
    FILE* file = fopen(filename, "rb");
    if (!file) {
        pthread_mutex_unlock(&ctx->lock);
        return false;
    }
    
    // Read header
    StateHeader header;
    if (fread(&header, sizeof(StateHeader), 1, file) != 1) {
        fclose(file);
        pthread_mutex_unlock(&ctx->lock);
        return false;
    }
    
    // Validate header
    if (header.magic != MAGIC_NUMBER || header.version > ctx->header.version) {
        fclose(file);
        pthread_mutex_unlock(&ctx->lock);
        return false;
    }
    
    // Read state data
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file) - sizeof(StateHeader);
    fseek(file, sizeof(StateHeader), SEEK_SET);
    
    void* new_cache = malloc(file_size);
    if (!new_cache) {
        fclose(file);
        pthread_mutex_unlock(&ctx->lock);
        return false;
    }
    
    if (fread(new_cache, file_size, 1, file) != 1) {
        free(new_cache);
        fclose(file);
        pthread_mutex_unlock(&ctx->lock);
        return false;
    }
    
    // Verify checksum
    uint32_t checksum = calculate_checksum(new_cache, file_size);
    if (checksum != header.checksum) {
        free(new_cache);
        fclose(file);
        pthread_mutex_unlock(&ctx->lock);
        return false;
    }
    
    // Update state
    free(ctx->cache);
    ctx->cache = new_cache;
    ctx->cache_size = file_size;
    memcpy(&ctx->header, &header, sizeof(StateHeader));
    
    fclose(file);
    pthread_mutex_unlock(&ctx->lock);
    
    // Notify handlers
    for (size_t i = 0; i < ctx->handler_count; i++) {
        if (ctx->handlers[i].handler) {
            ctx->handlers[i].handler(program, ctx->handlers[i].type, NULL);
        }
    }
    
    return true;
}

// Register state change handler
static bool register_handler(Program* program, StateType type, StateChangeHandlerFn handler) {
    if (!program || !handler) return false;
    
    StateContext* ctx = program->user_data;
    if (!ctx || ctx->handler_count >= MAX_HANDLERS) return false;
    
    pthread_mutex_lock(&ctx->lock);
    
    // Check for existing handler
    for (size_t i = 0; i < ctx->handler_count; i++) {
        if (ctx->handlers[i].type == type) {
            ctx->handlers[i].handler = handler;
            pthread_mutex_unlock(&ctx->lock);
            return true;
        }
    }
    
    // Add new handler
    ctx->handlers[ctx->handler_count].type = type;
    ctx->handlers[ctx->handler_count].handler = handler;
    ctx->handler_count++;
    
    pthread_mutex_unlock(&ctx->lock);
    return true;
}

// Get state entry
static bool get_entry(Program* program, StateType type, const char* id, StateEntry* entry) {
    if (!program || !id || !entry) return false;
    
    StateContext* ctx = program->user_data;
    if (!ctx || !ctx->cache) return false;
    
    pthread_mutex_lock(&ctx->lock);
    
    // Search cache for entry
    StateEntry* cached = ctx->cache;
    size_t entries = ctx->cache_size / sizeof(StateEntry);
    bool found = false;
    
    for (size_t i = 0; i < entries; i++) {
        if (cached[i].type == type && strcmp(cached[i].id, id) == 0) {
            memcpy(entry, &cached[i], sizeof(StateEntry));
            found = true;
            break;
        }
    }
    
    pthread_mutex_unlock(&ctx->lock);
    return found;
}

// Set state entry
static bool set_entry(Program* program, const StateEntry* entry) {
    if (!program || !entry) return false;
    
    StateContext* ctx = program->user_data;
    if (!ctx) return false;
    
    pthread_mutex_lock(&ctx->lock);
    
    // Check if entry exists
    if (ctx->cache) {
        StateEntry* cached = ctx->cache;
        size_t entries = ctx->cache_size / sizeof(StateEntry);
        
        for (size_t i = 0; i < entries; i++) {
            if (cached[i].type == entry->type && strcmp(cached[i].id, entry->id) == 0) {
                memcpy(&cached[i], entry, sizeof(StateEntry));
                pthread_mutex_unlock(&ctx->lock);
                
                // Notify handlers
                for (size_t j = 0; j < ctx->handler_count; j++) {
                    if (ctx->handlers[j].type == entry->type && ctx->handlers[j].handler) {
                        ctx->handlers[j].handler(program, entry->type, entry->id);
                    }
                }
                
                return true;
            }
        }
    }
    
    // Add new entry
    size_t new_size = ctx->cache_size + sizeof(StateEntry);
    void* new_cache = realloc(ctx->cache, new_size);
    if (!new_cache) {
        pthread_mutex_unlock(&ctx->lock);
        return false;
    }
    
    ctx->cache = new_cache;
    memcpy((char*)ctx->cache + ctx->cache_size, entry, sizeof(StateEntry));
    ctx->cache_size = new_size;
    
    pthread_mutex_unlock(&ctx->lock);
    
    // Notify handlers
    for (size_t i = 0; i < ctx->handler_count; i++) {
        if (ctx->handlers[i].type == entry->type && ctx->handlers[i].handler) {
            ctx->handlers[i].handler(program, entry->type, entry->id);
        }
    }
    
    return true;
}

// Delete state entry
static bool delete_entry(Program* program, StateType type, const char* id) {
    if (!program || !id) return false;
    
    StateContext* ctx = program->user_data;
    if (!ctx || !ctx->cache) return false;
    
    pthread_mutex_lock(&ctx->lock);
    
    // Find and remove entry
    StateEntry* cached = ctx->cache;
    size_t entries = ctx->cache_size / sizeof(StateEntry);
    bool found = false;
    
    for (size_t i = 0; i < entries; i++) {
        if (cached[i].type == type && strcmp(cached[i].id, id) == 0) {
            // Move remaining entries
            if (i < entries - 1) {
                memmove(&cached[i], &cached[i + 1], 
                       (entries - i - 1) * sizeof(StateEntry));
            }
            ctx->cache_size -= sizeof(StateEntry);
            found = true;
            break;
        }
    }
    
    pthread_mutex_unlock(&ctx->lock);
    
    // Notify handlers if entry was deleted
    if (found) {
        for (size_t i = 0; i < ctx->handler_count; i++) {
            if (ctx->handlers[i].type == type && ctx->handlers[i].handler) {
                ctx->handlers[i].handler(program, type, id);
            }
        }
    }
    
    return found;
}

// State interface instance
static StateInterface state_interface = {
    .save = save_state,
    .load = load_state,
    .merge = NULL,  // Not implemented yet
    .set_entry = set_entry,
    .get_entry = get_entry,
    .delete_entry = delete_entry,
    .register_handler = register_handler,
    .verify = NULL,  // Not implemented yet
    .is_compatible = NULL  // Not implemented yet
};

// Initialize state interface
bool state_init(Program* program, const char* filename) {
    StateContext* ctx = calloc(1, sizeof(StateContext));
    if (!ctx) return false;
    
    if (!init_state_context(ctx, filename)) {
        free(ctx);
        return false;
    }
    
    program->user_data = ctx;
    return true;
}

// Cleanup state interface
void state_cleanup(Program* program) {
    if (!program) return;
    
    StateContext* ctx = program->user_data;
    if (ctx) {
        pthread_mutex_lock(&ctx->lock);
        free(ctx->filename);
        free(ctx->cache);
        pthread_mutex_unlock(&ctx->lock);
        pthread_mutex_destroy(&ctx->lock);
        free(ctx);
    }
}

// Get state interface
const StateInterface* get_state_interface(void) {
    return &state_interface;
}