
#include "phantomid.h"
#include "network.h"
#define QUEUE_SIZE 1000

// Static globals
static char error_buffer[256] = {0};

// Queue for BFS traversal
typedef struct {
    PhantomNode* nodes[QUEUE_SIZE];
    size_t front;
    size_t rear;
    size_t size;
} NodeQueue;

// Queue operations
static void queue_init(NodeQueue* q) {
    q->front = 0;
    q->rear = 0;
    q->size = 0;
}


// Forward declarations of callback functions
void on_client_data(NetworkEndpoint* endpoint, NetworkPacket* packet);
void on_client_connect(NetworkEndpoint* endpoint);
void on_client_disconnect(NetworkEndpoint* endpoint);

static bool queue_push(NodeQueue* q, PhantomNode* node) {
    if (q->size >= QUEUE_SIZE) return false;
    q->nodes[q->rear] = node;
    q->rear = (q->rear + 1) % QUEUE_SIZE;
    q->size++;
    return true;
}

static PhantomNode* queue_pop(NodeQueue* q) {
    if (q->size == 0) return NULL;
    PhantomNode* node = q->nodes[q->front];
    q->front = (q->front + 1) % QUEUE_SIZE;
    q->size--;
    return node;
}

// Generate cryptographic seed
static void generate_seed(uint8_t* seed) {
    RAND_bytes(seed, 32);
}

// Generate anonymous ID
static void generate_id(const uint8_t* seed, char* id) {
    unsigned int len;
    uint8_t hash[EVP_MAX_MD_SIZE];
    
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (ctx) {
        EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
        EVP_DigestUpdate(ctx, seed, 32);
        EVP_DigestFinal_ex(ctx, hash, &len);
        EVP_MD_CTX_free(ctx);
        
        for (unsigned int i = 0; i < 32; i++) {
            sprintf(&id[i * 2], "%02x", hash[i]);
        }
        id[64] = '\0';
    }
}

// Create new node
static PhantomNode* create_node(const PhantomAccount* account, bool is_root) {
    PhantomNode* node = calloc(1, sizeof(PhantomNode));
    if (!node) {
        snprintf(error_buffer, sizeof(error_buffer), "Failed to allocate node");
        return NULL;
    }
    
    node->children = calloc(MAX_CHILDREN, sizeof(PhantomNode*));
    if (!node->children) {
        snprintf(error_buffer, sizeof(error_buffer), "Failed to allocate children array");
        free(node);
        return NULL;
    }
    
    memcpy(&node->account, account, sizeof(PhantomAccount));
    node->parent = NULL;
    node->child_count = 0;
    node->max_children = MAX_CHILDREN;
    node->is_root = is_root;
    node->is_admin = is_root;
    pthread_mutex_init(&node->node_lock, NULL);
    
    return node;
}

// Tree initialization
bool phantom_tree_init(PhantomDaemon* phantom) {
    phantom->tree = calloc(1, sizeof(PhantomTree));
    if (!phantom->tree) {
        snprintf(error_buffer, sizeof(error_buffer), "Failed to allocate tree");
        return false;
    }
    
    phantom->tree->root = NULL;
    phantom->tree->total_nodes = 0;
    pthread_mutex_init(&phantom->tree->tree_lock, NULL);
    return true;
}

// Recursive cleanup helper
static void cleanup_node(PhantomNode* node) {
    if (!node) return;
    
    for (size_t i = 0; i < node->child_count; i++) {
        cleanup_node(node->children[i]);
    }
    
    pthread_mutex_destroy(&node->node_lock);
    free(node->children);
    free(node);
}

// Tree cleanup
void phantom_tree_cleanup(PhantomDaemon* phantom) {
    if (!phantom || !phantom->tree) return;
    
    pthread_mutex_lock(&phantom->tree->tree_lock);
    cleanup_node(phantom->tree->root);
    pthread_mutex_unlock(&phantom->tree->tree_lock);
    
    pthread_mutex_destroy(&phantom->tree->tree_lock);
    free(phantom->tree);
    phantom->tree = NULL;
}

// Find node by ID
PhantomNode* phantom_tree_find(PhantomDaemon* phantom, const char* id) {
    if (!phantom || !phantom->tree || !id) return NULL;
    
    NodeQueue queue;
    queue_init(&queue);
    
    pthread_mutex_lock(&phantom->tree->tree_lock);
    
    if (!phantom->tree->root) {
        pthread_mutex_unlock(&phantom->tree->tree_lock);
        return NULL;
    }
    
    queue_push(&queue, phantom->tree->root);
    
    while (queue.size > 0) {
        PhantomNode* node = queue_pop(&queue);
        pthread_mutex_lock(&node->node_lock);
        
        if (strcmp(node->account.id, id) == 0) {
            pthread_mutex_unlock(&node->node_lock);
            pthread_mutex_unlock(&phantom->tree->tree_lock);
            return node;
        }
        
        for (size_t i = 0; i < node->child_count; i++) {
            queue_push(&queue, node->children[i]);
        }
        
        pthread_mutex_unlock(&node->node_lock);
    }
    
    pthread_mutex_unlock(&phantom->tree->tree_lock);
    return NULL;
}

// Insert node into tree
PhantomNode* phantom_tree_insert(PhantomDaemon* phantom, const PhantomAccount* account, const char* parent_id) {
    if (!phantom || !phantom->tree || !account) {
        snprintf(error_buffer, sizeof(error_buffer), "Invalid parameters");
        return NULL;
    }
    
    pthread_mutex_lock(&phantom->tree->tree_lock);
    
    // Create root if tree is empty
    if (!phantom->tree->root) {
        if (parent_id) {
            pthread_mutex_unlock(&phantom->tree->tree_lock);
            snprintf(error_buffer, sizeof(error_buffer), "Cannot specify parent for root node");
            return NULL;
        }
        
        phantom->tree->root = create_node(account, true);
        if (phantom->tree->root) {
            phantom->tree->total_nodes = 1;
        }
        
        pthread_mutex_unlock(&phantom->tree->tree_lock);
        return phantom->tree->root;
    }
    
    // Find parent node
    PhantomNode* parent = parent_id ? phantom_tree_find(phantom, parent_id) : phantom->tree->root;
    if (!parent) {
        pthread_mutex_unlock(&phantom->tree->tree_lock);
        snprintf(error_buffer, sizeof(error_buffer), "Parent node not found");
        return NULL;
    }
    
    pthread_mutex_lock(&parent->node_lock);
    
    // Check if parent can accept more children
    if (parent->child_count >= parent->max_children) {
        pthread_mutex_unlock(&parent->node_lock);
        pthread_mutex_unlock(&phantom->tree->tree_lock);
        snprintf(error_buffer, sizeof(error_buffer), "Parent node full");
        return NULL;
    }
    
    // Create and insert new node
    PhantomNode* node = create_node(account, false);
    if (node) {
        node->parent = parent;
        parent->children[parent->child_count++] = node;
        phantom->tree->total_nodes++;
    }
    
    pthread_mutex_unlock(&parent->node_lock);
    pthread_mutex_unlock(&phantom->tree->tree_lock);
    
    return node;
}

// Delete node from tree
bool phantom_tree_delete(PhantomDaemon* phantom, const char* id) {
    if (!phantom || !phantom->tree || !id) return false;
    
    pthread_mutex_lock(&phantom->tree->tree_lock);
    
    PhantomNode* node = phantom_tree_find(phantom, id);
    if (!node) {
        pthread_mutex_unlock(&phantom->tree->tree_lock);
        return false;
    }
    
    pthread_mutex_lock(&node->node_lock);
    
    // Cannot delete root if it has children
    if (node->is_root && node->child_count > 0) {
        pthread_mutex_unlock(&node->node_lock);
        pthread_mutex_unlock(&phantom->tree->tree_lock);
        snprintf(error_buffer, sizeof(error_buffer), "Cannot delete root with children");
        return false;
    }
    
    // Update parent's children array
    if (node->parent) {
        pthread_mutex_lock(&node->parent->node_lock);
        
        for (size_t i = 0; i < node->parent->child_count; i++) {
            if (node->parent->children[i] == node) {
                // Shift remaining children left
                memmove(&node->parent->children[i],
                       &node->parent->children[i + 1],
                       (node->parent->child_count - i - 1) * sizeof(PhantomNode*));
                node->parent->child_count--;
                break;
            }
        }
        
        pthread_mutex_unlock(&node->parent->node_lock);
    } else {
        phantom->tree->root = NULL;
    }
    
    // Redistribute node's children
    for (size_t i = 0; i < node->child_count; i++) {
        PhantomNode* child = node->children[i];
        pthread_mutex_lock(&child->node_lock);
        
        child->parent = node->parent;
        child->is_admin = node->is_admin; // Inherit admin status
        
        if (node->parent) {
            pthread_mutex_lock(&node->parent->node_lock);
            node->parent->children[node->parent->child_count++] = child;
            pthread_mutex_unlock(&node->parent->node_lock);
        }
        
        pthread_mutex_unlock(&child->node_lock);
    }
    
    pthread_mutex_unlock(&node->node_lock);
    
    // Cleanup node
    pthread_mutex_destroy(&node->node_lock);
    free(node->children);
    free(node);
    
    phantom->tree->total_nodes--;
    
    pthread_mutex_unlock(&phantom->tree->tree_lock);
    return true;
}

// BFS traversal
void phantom_tree_bfs(PhantomDaemon* phantom, TreeVisitor visitor, void* user_data) {
    if (!phantom || !phantom->tree || !visitor) return;
    
    NodeQueue queue;
    queue_init(&queue);
    
    pthread_mutex_lock(&phantom->tree->tree_lock);
    
    if (!phantom->tree->root) {
        pthread_mutex_unlock(&phantom->tree->tree_lock);
        return;
    }
    
    queue_push(&queue, phantom->tree->root);
    
    while (queue.size > 0) {
        PhantomNode* node = queue_pop(&queue);
        pthread_mutex_lock(&node->node_lock);
        
        visitor(node, user_data);
        
        for (size_t i = 0; i < node->child_count; i++) {
            queue_push(&queue, node->children[i]);
        }
        
        pthread_mutex_unlock(&node->node_lock);
    }
    
    pthread_mutex_unlock(&phantom->tree->tree_lock);
}

// DFS traversal helper
static void dfs_helper(PhantomNode* node, TreeVisitor visitor, void* user_data) {
    if (!node) return;
    
    pthread_mutex_lock(&node->node_lock);
    
    visitor(node, user_data);
    
    for (size_t i = 0; i < node->child_count; i++) {
        dfs_helper(node->children[i], visitor, user_data);
    }
    
    pthread_mutex_unlock(&node->node_lock);
}

// DFS traversal
void phantom_tree_dfs(PhantomDaemon* phantom, TreeVisitor visitor, void* user_data) {
    if (!phantom || !phantom->tree || !visitor) return;
    
    pthread_mutex_lock(&phantom->tree->tree_lock);
    
    if (phantom->tree->root) {
        dfs_helper(phantom->tree->root, visitor, user_data);
    }
    
    pthread_mutex_unlock(&phantom->tree->tree_lock);
}

// Tree status functions
bool phantom_tree_has_root(const PhantomDaemon* phantom) {
    if (!phantom || !phantom->tree) return false;
    return phantom->tree->root != NULL;
}

size_t phantom_tree_size(const PhantomDaemon* phantom) {
    if (!phantom || !phantom->tree) return 0;
    return phantom->tree->total_nodes;
}

// Calculate tree depth helper
static size_t get_depth_helper(PhantomNode* node) {
    if (!node) return 0;
    
    size_t max_depth = 0;
    for (size_t i = 0; i < node->child_count; i++) {
        size_t depth = get_depth_helper(node->children[i]);
        if (depth > max_depth) max_depth = depth;
    }
    
    return max_depth + 1;
}

// Calculate tree depth
size_t phantom_tree_depth(const PhantomDaemon* phantom) {
    if (!phantom || !phantom->tree || !phantom->tree->root) return 0;
    return get_depth_helper(phantom->tree->root);
}

// Print tree helper
static void print_node(PhantomNode* node, void* user_data) {
    int* level = (int*)user_data;
    for (int i = 0; i < *level; i++) printf("  ");
    printf("- %s (%s, %s)\n", node->account.id,
           node->is_root ? "Root" : "Child",
           node->is_admin ? "Admin" : "User");
}

// Print tree structure
void phantom_tree_print(const PhantomDaemon* phantom) {
    if (!phantom || !phantom->tree) return;
    
    printf("PhantomID Tree Structure:\n");
    int level = 0;
    phantom_tree_dfs((PhantomDaemon*)phantom, print_node, &level);
}



// Initialize PhantomID daemon
bool phantom_init(PhantomDaemon* phantom, uint16_t port, bool enable_history) {
    if (!phantom) return false;

    memset(phantom, 0, sizeof(PhantomDaemon));  // Clear all fields
    phantom->max_admins = 5;  // Default value

    // Initialize history
    phantom_history_init(phantom->history, enable_history);
    pthread_mutex_init(&phantom->state_lock, NULL);
    
    if (!phantom_tree_init(phantom)) {
        return false;
    }
    
    // Initialize network server
    NetworkEndpoint server = {
        .address = "0.0.0.0",
        .port = port,
        .protocol = NET_TCP,
        .role = NET_SERVER,
        .mode = NET_BLOCKING,
        .phantom = phantom
    };
    
    phantom->network.endpoints = malloc(sizeof(NetworkEndpoint));
    if (!phantom->network.endpoints) {
        phantom_tree_cleanup(phantom);
        return false;
    }
    
    memcpy(phantom->network.endpoints, &server, sizeof(NetworkEndpoint));
    phantom->network.count = 1;
    phantom->network.phantom = phantom;
    
    // Set up handlers
    phantom->network.handlers.on_connect = phantom_on_client_connect;
    phantom->network.handlers.on_disconnect = phantom_on_client_disconnect;
    phantom->network.handlers.on_receive = phantom_on_client_data;
    
    if (!net_init(&phantom->network.endpoints[0])) {
        phantom_tree_cleanup(phantom);
        free(phantom->network.endpoints);
        return false;
    }
    
    return true;
}
void phantom_cleanup(PhantomDaemon* phantom) {
    if (!phantom) return;
    
    pthread_mutex_lock(&phantom->state_lock);
    phantom->running = false;
    
    // Cleanup tree
    phantom_tree_cleanup(phantom);
    
    // Cleanup network resources
    if (phantom->network.endpoints) {
        for (size_t i = 0; i < phantom->network.count; i++) {
            net_close(&phantom->network.endpoints[i]);
        }
        free(phantom->network.endpoints);
    }
    
    pthread_mutex_unlock(&phantom->state_lock);
    pthread_mutex_destroy(&phantom->state_lock);
}



// Network callbacks implementation
// Simplified command handling and updated msg API
void phantom_on_client_data(NetworkEndpoint* endpoint, NetworkPacket* packet) {
     char* data = (char*)packet->data;
    data[packet->size] = '\0';

    char response[MAX_MESSAGE_SIZE] = {0};
    NetworkPacket resp = {
        .data = response,
        .size = sizeof(response),
        .flags = 0
    };
// Define the visitor function
    void tree_visitor(PhantomNode* node, void* user_data) {
        char* buffer = (char*)user_data;
        snprintf(buffer + strlen(buffer), MAX_MESSAGE_SIZE - strlen(buffer),
                 "- ID: %s | Role: %s\n", node->account.id,
                 node->is_admin ? "Admin" : (node->is_root ? "Root" : "Child"));
    }

    if (strncmp(data, "list bfs", 8) == 0) {
        phantom_tree_bfs(endpoint->phantom, tree_visitor, response);
    } else {
        phantom_tree_dfs(endpoint->phantom, tree_visitor, response);
    }

    if (endpoint->phantom->history->enabled) {
        pthread_mutex_lock(&endpoint->phantom->history->lock);
        for (size_t i = 0; i < endpoint->phantom->history->size; i++) {
            strcat(response, endpoint->phantom->history->entries[i]);
            strcat(response, "\n");
        }
        pthread_mutex_unlock(&endpoint->phantom->history->lock);
    }
    // Simplified message command
    if (strncmp(data, "msg", 3) == 0) {
        char from_id[65], to_id[65], message[MAX_MESSAGE_SIZE];
        if (sscanf(data + 4, "%64s %64s <%[^>]>", from_id, to_id, message) == 3) {
            if (phantom_message_send(endpoint->phantom, from_id, to_id, message)) {
                snprintf(response, sizeof(response),
                         "\nMessage sent successfully from %s to %s\n", from_id, to_id);
            } else {
                snprintf(response, sizeof(response),
                         "\nFailed to send message: %s\n", phantom_get_error());
            }
        } else {
            snprintf(response, sizeof(response),
                     "\nInvalid message format. Use: msg <from_id> <to_id> <message>\n");
        }
    }

    // Implement create command with max_admins restriction
    else if (strncmp(data, "create", 6) == 0) {
        char parent_id[65] = {0};
        bool is_root = false;
        PhantomAccount account = {0};
        generate_seed(account.seed);
        generate_id(account.seed, account.id);
        account.creation_time = time(NULL);
        account.expiry_time = account.creation_time + (90 * 24 * 60 * 60);

        // Check max_admins limit
        if (endpoint->phantom->current_admin_count < endpoint->phantom->max_admins) {
            is_root = true;  // Assign admin/root privileges
            endpoint->phantom->current_admin_count++;
        }

        PhantomNode* node = phantom_tree_insert(endpoint->phantom, &account, is_root ? NULL : parent_id);
        if (node) {
            snprintf(response, sizeof(response),
                     "\nAccount created:\nID: %s\nRole: %s\n",
                     account.id, is_root ? "Admin" : "Child");
        } else {
            snprintf(response, sizeof(response),
                     "\nFailed to create account: %s\n",
                     phantom_get_error());
        }
    } 


    if (strncmp(data, "list bfs", 8) == 0) {
        phantom_tree_bfs(endpoint->phantom, TreeVisitor, response);
    } else {
        phantom_tree_dfs(endpoint->phantom, TreeVisitor, response);
    }

    // Append history to the response if enabled
    if (endpoint->phantom->history->enabled) {
        strcat(response, "\nUser History:\n");
        pthread_mutex_lock(&endpoint->phantom->history->lock);
        for (size_t i = 0; i < endpoint->phantom->history->size; i++) {
            strcat(response, endpoint->phantom->history->entries[i]);
            strcat(response, "\n");
        }
        pthread_mutex_unlock(&endpoint->phantom->history->lock);
    }

    // Handle orphaned accounts in list BFS/DFS
    else if (strncmp(data, "list bfs", 8) == 0 || strncmp(data, "list dfs", 8) == 0) {
    snprintf(response, sizeof(response), "\nTree Structure (%s):\n",
             strncmp(data, "list bfs", 8) == 0 ? "BFS" : "DFS");



// Use the function pointer
if (strncmp(data, "list bfs", 8) == 0) {
    phantom_tree_bfs(endpoint->phantom, tree_visitor, response);
} else {
    phantom_tree_dfs(endpoint->phantom, tree_visitor, response);
}



    if (strncmp(data, "list bfs", 8) == 0) {
        phantom_tree_bfs(endpoint->phantom, visitor, response);
    } else {
        phantom_tree_dfs(endpoint->phantom, visitor, response);
    }

    // Append history to the response if enabled
    if (endpoint->phantom->history.enabled) {
        strcat(response, "\nUser History:\n");
        pthread_mutex_lock(&endpoint->phantom->history.lock);
        for (size_t i = 0; i < endpoint->phantom->history.size; i++) {
            strcat(response, endpoint->phantom->history.entries[i]);
            strcat(response, "\n");
        }
        pthread_mutex_unlock(&endpoint->phantom->history.lock);
    }
}

    // Send response back to client
    if (resp.size == 0) {
        resp.size = strlen(response);
    }
    if (net_send(endpoint, &resp) < 0) {
        printf("Failed to send response to client\n");
    }
}

// Network callbacks with proper usage of parameters
void phantom_on_client_connect(NetworkEndpoint* endpoint) {
    char addr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(endpoint->addr.sin_addr), addr, INET_ADDRSTRLEN);
    printf("New client connected from %s:%d\n", addr, 
           ntohs(endpoint->addr.sin_port));
}

void phantom_on_client_disconnect(NetworkEndpoint* endpoint) {
    char addr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(endpoint->addr.sin_addr), addr, INET_ADDRSTRLEN);
    printf("Client disconnected from %s:%d\n", addr, 
           ntohs(endpoint->addr.sin_port));
}

// Run daemon
void phantom_run(PhantomDaemon* phantom) {
    if (!phantom) return;
    
    printf("PhantomID daemon running...\n");
    phantom->running = true;  // Set running flag

    NetworkProgram* network = &phantom->network;
    network->running = true;  // Set network running flag
    
    while (phantom->running) {
        net_run(network);  // This should block until there's activity
        if (!phantom->running) break;  // Check if we should stop
    }

    network->running = false;  // Clear network running flag
    printf("PhantomID daemon stopped\n");
}

// Message sending implementation
bool phantom_message_send(PhantomDaemon* phantom, const char* from_id,
                         const char* to_id, const char* content) {
    if (!phantom || !from_id || !to_id || !content) {
        snprintf(error_buffer, sizeof(error_buffer), "Invalid parameters");
        return false;
    }
    
    // Verify both nodes exist
    PhantomNode* from_node = phantom_tree_find(phantom, from_id);
    PhantomNode* to_node = phantom_tree_find(phantom, to_id);
    
    if (!from_node || !to_node) {
        snprintf(error_buffer, sizeof(error_buffer), "Source or destination node not found");
        return false;
    }
    
    // In a real implementation, I'd queue the message for delivery
    // For now, just log it
    printf("Message from %s to %s: %s\n", from_id, to_id, content);
    return true;
}

// Get messages for a node
PhantomMessage* phantom_message_get(PhantomDaemon* phantom, const char* id,
                                  size_t* count) {    if (!phantom || !id || !count) {
        snprintf(error_buffer, sizeof(error_buffer), "Invalid parameters");
        return NULL;
    }
    
    // Verify node exists
    PhantomNode* node = phantom_tree_find(phantom, id);
    if (!node) {
        snprintf(error_buffer, sizeof(error_buffer), "Node not found");
        return NULL;
    }
    
    // TODO:Add Queued message
    // In a real implementation, you'd retrieve queued messages
    // For now, return nothing
    *count = 0;
    return NULL;
}

// Error handling
const char* phantom_get_error(void) {
    return error_buffer;
}

// Time utility
time_t phantom_get_time(void) {
    return time(NULL);
}