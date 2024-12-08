#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "tree.h"

#define MAX_QUEUE_SIZE 1000

// Queue for BFS traversal
typedef struct {
    TreeNode* nodes[MAX_QUEUE_SIZE];
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

static bool queue_push(NodeQueue* q, TreeNode* node) {
    if (q->size >= MAX_QUEUE_SIZE) return false;
    q->nodes[q->rear] = node;
    q->rear = (q->rear + 1) % MAX_QUEUE_SIZE;
    q->size++;
    return true;
}

static TreeNode* queue_pop(NodeQueue* q) {
    if (q->size == 0) return NULL;
    TreeNode* node = q->nodes[q->front];
    q->front = (q->front + 1) % MAX_QUEUE_SIZE;
    q->size--;
    return node;
}

// Create new tree context
TreeContext* tree_create(void) {
    TreeContext* ctx = calloc(1, sizeof(TreeContext));
    if (!ctx) return NULL;

    if (pthread_mutex_init(&ctx->lock, NULL) != 0) {
        free(ctx);
        return NULL;
    }

    ctx->root = NULL;
    ctx->total_nodes = 0;
    return ctx;
}

// Create new node
static TreeNode* create_node(void) {
    TreeNode* node = calloc(1, sizeof(TreeNode));
    if (!node) return NULL;

    node->children = calloc(MAX_CHILDREN, sizeof(TreeNode*));
    if (!node->children) {
        free(node);
        return NULL;
    }

    node->creation_time = time(NULL);
    node->is_active = true;
    node->max_children = MAX_CHILDREN;
    node->child_count = 0;
    node->parent = NULL;
    
    return node;
}

// Generate unique node ID
static void generate_node_id(char* id) {
    static uint64_t counter = 0;
    sprintf(id, "node_%lu_%lu", (unsigned long)time(NULL), 
            (unsigned long)__sync_fetch_and_add(&counter, 1));
}

// Create new node in tree
TreeNode* tree_create_node(TreeContext* ctx, const char* parent_id) {
    if (!ctx) return NULL;

    pthread_mutex_lock(&ctx->lock);

    TreeNode* node = create_node();
    if (!node) {
        pthread_mutex_unlock(&ctx->lock);
        return NULL;
    }

    generate_node_id(node->id);

    // Handle root node creation
    if (!parent_id || !ctx->root) {
        if (!ctx->root) {
            node->is_root = true;
            ctx->root = node;
        } else {
            // Cannot create rootless node if root exists
            pthread_mutex_unlock(&ctx->lock);
            free(node->children);
            free(node);
            return NULL;
        }
    } else {
        // Find parent node
        TreeNode* parent = tree_find_node(ctx, parent_id);
        if (!parent || parent->child_count >= parent->max_children) {
            pthread_mutex_unlock(&ctx->lock);
            free(node->children);
            free(node);
            return NULL;
        }

        // Attach to parent
        node->parent = parent;
        parent->children[parent->child_count++] = node;
    }

    ctx->total_nodes++;
    pthread_mutex_unlock(&ctx->lock);
    return node;
}

// Find node by ID
TreeNode* tree_find_node(TreeContext* ctx, const char* node_id) {
    if (!ctx || !node_id || !ctx->root) return NULL;

    NodeQueue queue;
    queue_init(&queue);
    queue_push(&queue, ctx->root);

    while (queue.size > 0) {
        TreeNode* node = queue_pop(&queue);
        if (strcmp(node->id, node_id) == 0) {
            return node;
        }

        for (size_t i = 0; i < node->child_count; i++) {
            queue_push(&queue, node->children[i]);
        }
    }

    return NULL;
}

// Handle orphaned children during node deletion
static void handle_orphans(TreeContext* ctx, TreeNode* node) {
    if (!node || node->child_count == 0) return;

    // Try to reassign children to grandparent if exists
    if (node->parent) {
        for (size_t i = 0; i < node->child_count; i++) {
            TreeNode* child = node->children[i];
            if (node->parent->child_count < node->parent->max_children) {
                child->parent = node->parent;
                node->parent->children[node->parent->child_count++] = child;
            } else {
                // Make child a new root if no space in grandparent
                child->parent = NULL;
                child->is_root = true;
            }
        }
    } else {
        // Parent was root, children become new roots
        for (size_t i = 0; i < node->child_count; i++) {
            TreeNode* child = node->children[i];
            child->parent = NULL;
            child->is_root = true;
        }
    }
}

// Delete node from tree
bool tree_delete_node(TreeContext* ctx, const char* node_id) {
    if (!ctx || !node_id) return false;

    pthread_mutex_lock(&ctx->lock);

    TreeNode* node = tree_find_node(ctx, node_id);
    if (!node) {
        pthread_mutex_unlock(&ctx->lock);
        return false;
    }

    // Remove from parent's children array
    if (node->parent) {
        size_t index = 0;
        for (; index < node->parent->child_count; index++) {
            if (node->parent->children[index] == node) break;
        }
        
        // Shift remaining children left
        memmove(&node->parent->children[index],
                &node->parent->children[index + 1],
                (node->parent->child_count - index - 1) * sizeof(TreeNode*));
        node->parent->child_count--;
    } else if (node == ctx->root) {
        ctx->root = NULL;
    }

    // Handle orphaned children
    handle_orphans(ctx, node);

    // Free node
    free(node->children);
    free(node);
    ctx->total_nodes--;

    pthread_mutex_unlock(&ctx->lock);
    return true;
}

// BFS traversal
void tree_traverse_bfs(TreeContext* ctx, TreeVisitor visitor, void* user_data) {
    if (!ctx || !visitor || !ctx->root) return;

    pthread_mutex_lock(&ctx->lock);

    NodeQueue queue;
    queue_init(&queue);
    queue_push(&queue, ctx->root);

    while (queue.size > 0) {
        TreeNode* node = queue_pop(&queue);
        visitor(node, user_data);

        for (size_t i = 0; i < node->child_count; i++) {
            queue_push(&queue, node->children[i]);
        }
    }

    pthread_mutex_unlock(&ctx->lock);
}

// DFS traversal helper
static void dfs_helper(TreeNode* node, TreeVisitor visitor, void* user_data) {
    if (!node) return;
    
    visitor(node, user_data);
    
    for (size_t i = 0; i < node->child_count; i++) {
        dfs_helper(node->children[i], visitor, user_data);
    }
}

// DFS traversal
void tree_traverse_dfs(TreeContext* ctx, TreeVisitor visitor, void* user_data) {
    if (!ctx || !visitor || !ctx->root) return;

    pthread_mutex_lock(&ctx->lock);
    dfs_helper(ctx->root, visitor, user_data);
    pthread_mutex_unlock(&ctx->lock);
}

// Check if tree has root
bool tree_has_root(const TreeContext* ctx) {
    if (!ctx) return false;
    return ctx->root != NULL;
}

// Get total node count
size_t tree_get_size(const TreeContext* ctx) {
    if (!ctx) return 0;
    return ctx->total_nodes;
}

// Calculate tree depth helper
static size_t get_depth_helper(const TreeNode* node) {
    if (!node) return 0;

    size_t max_depth = 0;
    for (size_t i = 0; i < node->child_count; i++) {
        size_t depth = get_depth_helper(node->children[i]);
        if (depth > max_depth) max_depth = depth;
    }

    return max_depth + 1;
}

// Get tree depth
size_t tree_get_depth(const TreeContext* ctx) {
    if (!ctx || !ctx->root) return 0;
    return get_depth_helper(ctx->root);
}

// Cleanup node recursively
static void cleanup_node(TreeNode* node) {
    if (!node) return;

    for (size_t i = 0; i < node->child_count; i++) {
        cleanup_node(node->children[i]);
    }

    free(node->children);
    free(node);
}

// Destroy tree context
void tree_destroy(TreeContext* ctx) {
    if (!ctx) return;

    pthread_mutex_lock(&ctx->lock);
    cleanup_node(ctx->root);
    pthread_mutex_unlock(&ctx->lock);
    
    pthread_mutex_destroy(&ctx->lock);
    free(ctx);
}