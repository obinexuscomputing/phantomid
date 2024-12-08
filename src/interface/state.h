#ifndef STATE_INTERFACE_H
#define STATE_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "program.h"

// State types
typedef enum {
    STATE_PROGRAM = 0,  // Program state
    STATE_NODE,         // Node state
    STATE_NETWORK,      // Network state
    STATE_CUSTOM = 1000 // Base for program-specific states
} StateType;

// State flags
typedef enum {
    STATE_FLAG_NONE = 0,
    STATE_FLAG_PERSISTENT = 1,    // Save to disk
    STATE_FLAG_REPLICATED = 2,    // Share with peers
    STATE_FLAG_ENCRYPTED = 4,     // Encrypt content
    STATE_FLAG_COMPRESSED = 8     // Compress content
} StateFlags;

// State header
typedef struct {
    uint32_t magic;              // Magic number
    uint32_t version;            // State version
    StateFlags flags;            // State flags
    time_t timestamp;            // State timestamp
    uint32_t checksum;          // State checksum
} StateHeader;

// State entry
typedef struct {
    StateType type;              // Entry type
    char id[64];                // Entry identifier
    void* data;                 // Entry data
    size_t data_size;          // Data size
} StateEntry;

// State change callback
typedef void (*StateChangeHandlerFn)(Program* program,
                                   StateType type,
                                   const char* id);

// State interface
typedef struct {
    // State operations
    bool (*save)(Program* program,
                const char* filename);
                
    bool (*load)(Program* program,
                const char* filename);
    
    bool (*merge)(Program* program,
                 const char* filename);
    
    // Entry management
    bool (*set_entry)(Program* program,
                     const StateEntry* entry);
                     
    bool (*get_entry)(Program* program,
                     StateType type,
                     const char* id,
                     StateEntry* entry);
                     
    bool (*delete_entry)(Program* program,
                        StateType type,
                        const char* id);
    
    // State monitoring
    bool (*register_handler)(Program* program,
                           StateType type,
                           StateChangeHandlerFn handler);
    
    // State validation
    bool (*verify)(Program* program,
                  const char* filename);
                  
    bool (*is_compatible)(Program* program,
                         const StateHeader* header);
} StateInterface;

#endif // STATE_INTERFACE_H