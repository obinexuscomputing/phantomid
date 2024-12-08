#include <stdio.h>
#include <assert.h>
#include "../../runtime/tree/tree.h"

// Test visitor function to print node info
static void print_node(TreeNode* node, void* depth_ptr) {
    int depth = depth_ptr ? *(int*)depth_ptr : 0;
    for (int i = 0; i < depth; i++) printf("  ");
    printf("- %s (Root: %s, Children: %zu/%zu)\n",
           node->id,
           node->is_root ? "Yes" : "No",
           node->child_count,
           node->max_children);
}

// Tests creation and basic properties
void test_tree_creation(void) {
    printf("\nTesting tree creation...\n");
    
    TreeContext* ctx = tree_create();
    assert(ctx != NULL);
    assert(tree_get_size(ctx) == 0);
    assert(tree_has_root(ctx) == false);
    assert(tree_get_depth(ctx) == 0);
    
    // Create root node
    TreeNode* root = tree_create_node(ctx, NULL);
    assert(root != NULL);
    assert(root->is_root == true);
    assert(tree_has_root(ctx) == true);
    assert(tree_get_size(ctx) == 1);
    assert(tree_get_depth(ctx) == 1);
    
    printf("Tree creation tests passed!\n");
    tree_destroy(ctx);
}

// Tests parent-child relationships
void test_tree_relationships(void) {
    printf("\nTesting tree relationships...\n");
    
    TreeContext* ctx = tree_create();
    TreeNode* root = tree_create_node(ctx, NULL);
    assert(root != NULL);
    
    // Add children to root
    TreeNode* child1 = tree_create_node(ctx, root->id);
    TreeNode* child2 = tree_create_node(ctx, root->id);
    assert(child1 != NULL && child2 != NULL);
    assert(child1->parent == root && child2->parent == root);
    assert(root->child_count == 2);
    
    // Add grandchild
    TreeNode* grandchild = tree_create_node(ctx, child1->id);
    assert(grandchild != NULL);
    assert(grandchild->parent == child1);
    assert(child1->child_count == 1);
    assert(tree_get_depth(ctx) == 3);
    
    printf("Tree relationship tests passed!\n");
    tree_destroy(ctx);
}

// Tests orphan handling
void test_orphan_handling(void) {
    printf("\nTesting orphan handling...\n");
    
    TreeContext* ctx = tree_create();
    TreeNode* root = tree_create_node(ctx, NULL);
    TreeNode* child1 = tree_create_node(ctx, root->id);
    TreeNode* child2 = tree_create_node(ctx, root->id);
    TreeNode* grandchild1 = tree_create_node(ctx, child1->id);
    TreeNode* grandchild2 = tree_create_node(ctx, child1->id);
    
    printf("Initial tree structure:\n");
    int depth = 0;
    tree_traverse_dfs(ctx, print_node, &depth);
    
    // Delete middle node (child1)
    printf("\nDeleting node %s...\n", child1->id);
    bool deleted = tree_delete_node(ctx, child1->id);
    assert(deleted);
    
    printf("\nTree structure after deletion:\n");
    tree_traverse_dfs(ctx, print_node, &depth);
    
    // Verify orphan handling
    TreeNode* found_grandchild1 = tree_find_node(ctx, grandchild1->id);
    TreeNode* found_grandchild2 = tree_find_node(ctx, grandchild2->id);
    assert(found_grandchild1 != NULL && found_grandchild2 != NULL);
    assert(found_grandchild1->parent == root || found_grandchild1->is_root);
    assert(found_grandchild2->parent == root || found_grandchild2->is_root);
    
    printf("Orphan handling tests passed!\n");
    tree_destroy(ctx);
}

// Tests tree traversal
void test_tree_traversal(void) {
    printf("\nTesting tree traversal...\n");
    
    TreeContext* ctx = tree_create();
    TreeNode* root = tree_create_node(ctx, NULL);
    
    // Create a more complex tree
    TreeNode* children[3];
    for (int i = 0; i < 3; i++) {
        children[i] = tree_create_node(ctx, root->id);
        for (int j = 0; j < 2; j++) {
            tree_create_node(ctx, children[i]->id);
        }
    }
    
    printf("\nBFS Traversal:\n");
    int depth = 0;
    tree_traverse_bfs(ctx, print_node, &depth);
    
    printf("\nDFS Traversal:\n");
    tree_traverse_dfs(ctx, print_node, &depth);
    
    printf("Tree traversal tests passed!\n");
    tree_destroy(ctx);
}

// Tests node finding
void test_node_finding(void) {
    printf("\nTesting node finding...\n");
    
    TreeContext* ctx = tree_create();
    TreeNode* root = tree_create_node(ctx, NULL);
    TreeNode* child = tree_create_node(ctx, root->id);
    
    // Test finding existing nodes
    TreeNode* found_root = tree_find_node(ctx, root->id);
    TreeNode* found_child = tree_find_node(ctx, child->id);
    assert(found_root == root);
    assert(found_child == child);
    
    // Test finding non-existent node
    TreeNode* not_found = tree_find_node(ctx, "nonexistent");
    assert(not_found == NULL);
    
    printf("Node finding tests passed!\n");
    tree_destroy(ctx);
}

int main(void) {
    printf("Starting tree tests...\n");

    test_tree_creation();
    test_tree_relationships();
    test_orphan_handling();
    test_tree_traversal();
    test_node_finding();

    printf("\nAll tests passed successfully!\n");
    return 0;
}