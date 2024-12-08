#ifndef TREE_H
#define TREE_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// Tree node representing a person in the network
typedef struct TreeNode {
    char id[64];                  // Unique node identifier
    time_t creation_time;         // When node was created
    bool is_root;                 // If this is a root node
    bool is_active;               // If node is currently active
    struct TreeNode* parent;      // Parent node reference
    struct TreeNode** children;   // Array of child nodes
    size_t child_count;           // Number of children
    size_t max_children;          // Maximum allowed children
    void* user_data;             // Custom data attachment
} TreeNode;

// Tree context managing all nodes
typedef struct TreeContext {
    TreeNode* root;               // Root node of tree
    size_t total_nodes;           // Total number of nodes
    pthread_mutex_t lock;         // Thread safety lock
} TreeContext;

// Basic tree operations
TreeContext* tree_create(void);
void tree_destroy(TreeContext* ctx);

// Node operations
TreeNode* tree_create_node(TreeContext* ctx, const char* parent_id);
bool tree_delete_node(TreeContext* ctx, const char* node_id);
TreeNode* tree_find_node(TreeContext* ctx, const char* node_id);

// Tree traversal callbacks
typedef void (*TreeVisitor)(TreeNode* node, void* user_data);

// Tree traversal
void tree_traverse_bfs(TreeContext* ctx, TreeVisitor visitor, void* user_data);
void tree_traverse_dfs(TreeContext* ctx, TreeVisitor visitor, void* user_data);

// Tree status
bool tree_has_root(const TreeContext* ctx);
size_t tree_get_size(const TreeContext* ctx);
size_t tree_get_depth(const TreeContext* ctx);

#endif // TREE_H