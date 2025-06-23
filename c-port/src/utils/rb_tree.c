#include "cloudterm.h"
#include <stdlib.h>
#include <assert.h>

#define RB_RED   0
#define RB_BLACK 1

/* Get parent node */
static inline ct_rb_node_t *rb_parent(ct_rb_node_t *node) {
    return node ? node->parent : NULL;
}

/* Check if node is red */
static inline int rb_is_red(ct_rb_node_t *node) {
    return node && node->color == RB_RED;
}

/* Check if node is black (NULL nodes are black) */
static inline int rb_is_black(ct_rb_node_t *node) {
    return !node || node->color == RB_BLACK;
}

/* Set node color */
static inline void rb_set_color(ct_rb_node_t *node, int color) {
    if (node) node->color = color;
}

/* Left rotation */
static void rb_rotate_left(ct_rb_node_t **root, ct_rb_node_t *node) {
    ct_rb_node_t *right = node->right;
    ct_rb_node_t *parent = rb_parent(node);
    
    node->right = right->left;
    if (right->left) {
        right->left->parent = node;
    }
    
    right->left = node;
    right->parent = parent;
    
    if (parent) {
        if (node == parent->left) {
            parent->left = right;
        } else {
            parent->right = right;
        }
    } else {
        *root = right;
    }
    
    node->parent = right;
}

/* Right rotation */
static void rb_rotate_right(ct_rb_node_t **root, ct_rb_node_t *node) {
    ct_rb_node_t *left = node->left;
    ct_rb_node_t *parent = rb_parent(node);
    
    node->left = left->right;
    if (left->right) {
        left->right->parent = node;
    }
    
    left->right = node;
    left->parent = parent;
    
    if (parent) {
        if (node == parent->right) {
            parent->right = left;
        } else {
            parent->left = left;
        }
    } else {
        *root = left;
    }
    
    node->parent = left;
}

/* Fix red-black properties after insertion */
static void rb_insert_fixup(ct_rb_node_t **root, ct_rb_node_t *node) {
    ct_rb_node_t *parent, *grandparent, *uncle;
    
    while ((parent = rb_parent(node)) && rb_is_red(parent)) {
        grandparent = rb_parent(parent);
        
        if (parent == grandparent->left) {
            uncle = grandparent->right;
            
            if (rb_is_red(uncle)) {
                /* Case 1: Uncle is red */
                rb_set_color(parent, RB_BLACK);
                rb_set_color(uncle, RB_BLACK);
                rb_set_color(grandparent, RB_RED);
                node = grandparent;
            } else {
                if (node == parent->right) {
                    /* Case 2: Node is right child */
                    rb_rotate_left(root, parent);
                    node = parent;
                    parent = rb_parent(node);
                }
                
                /* Case 3: Node is left child */
                rb_set_color(parent, RB_BLACK);
                rb_set_color(grandparent, RB_RED);
                rb_rotate_right(root, grandparent);
            }
        } else {
            /* Mirror cases */
            uncle = grandparent->left;
            
            if (rb_is_red(uncle)) {
                rb_set_color(parent, RB_BLACK);
                rb_set_color(uncle, RB_BLACK);
                rb_set_color(grandparent, RB_RED);
                node = grandparent;
            } else {
                if (node == parent->left) {
                    rb_rotate_right(root, parent);
                    node = parent;
                    parent = rb_parent(node);
                }
                
                rb_set_color(parent, RB_BLACK);
                rb_set_color(grandparent, RB_RED);
                rb_rotate_left(root, grandparent);
            }
        }
    }
    
    rb_set_color(*root, RB_BLACK);
}

/* Insert node into red-black tree */
void ct_rb_insert(ct_rb_node_t **root, ct_rb_node_t *node, 
                  int (*compare)(ct_rb_node_t *, ct_rb_node_t *)) {
    ct_rb_node_t *parent = NULL;
    ct_rb_node_t **link = root;
    
    /* Initialize node */
    node->left = node->right = NULL;
    node->color = RB_RED;
    
    /* Find insertion point - O(log n) */
    while (*link) {
        parent = *link;
        int cmp = compare(node, parent);
        
        if (cmp < 0) {
            link = &parent->left;
        } else {
            link = &parent->right;
        }
    }
    
    /* Insert node */
    node->parent = parent;
    *link = node;
    
    /* Fix red-black properties */
    rb_insert_fixup(root, node);
}

/* Find minimum node in subtree */
ct_rb_node_t *ct_rb_find_min(ct_rb_node_t *node) {
    if (!node) return NULL;
    
    while (node->left) {
        node = node->left;
    }
    
    return node;
}

/* Find successor node */
static ct_rb_node_t *rb_successor(ct_rb_node_t *node) {
    if (node->right) {
        return ct_rb_find_min(node->right);
    }
    
    ct_rb_node_t *parent = rb_parent(node);
    while (parent && node == parent->right) {
        node = parent;
        parent = rb_parent(parent);
    }
    
    return parent;
}

/* Fix red-black properties after deletion */
static void rb_delete_fixup(ct_rb_node_t **root, ct_rb_node_t *node, 
                           ct_rb_node_t *parent) {
    ct_rb_node_t *sibling;
    
    while (node != *root && rb_is_black(node)) {
        if (node == parent->left) {
            sibling = parent->right;
            
            if (rb_is_red(sibling)) {
                /* Case 1: Sibling is red */
                rb_set_color(sibling, RB_BLACK);
                rb_set_color(parent, RB_RED);
                rb_rotate_left(root, parent);
                sibling = parent->right;
            }
            
            if (rb_is_black(sibling->left) && rb_is_black(sibling->right)) {
                /* Case 2: Both children of sibling are black */
                rb_set_color(sibling, RB_RED);
                node = parent;
                parent = rb_parent(node);
            } else {
                if (rb_is_black(sibling->right)) {
                    /* Case 3: Sibling's right child is black */
                    rb_set_color(sibling->left, RB_BLACK);
                    rb_set_color(sibling, RB_RED);
                    rb_rotate_right(root, sibling);
                    sibling = parent->right;
                }
                
                /* Case 4: Sibling's right child is red */
                rb_set_color(sibling, parent->color);
                rb_set_color(parent, RB_BLACK);
                rb_set_color(sibling->right, RB_BLACK);
                rb_rotate_left(root, parent);
                node = *root;
                break;
            }
        } else {
            /* Mirror cases */
            sibling = parent->left;
            
            if (rb_is_red(sibling)) {
                rb_set_color(sibling, RB_BLACK);
                rb_set_color(parent, RB_RED);
                rb_rotate_right(root, parent);
                sibling = parent->left;
            }
            
            if (rb_is_black(sibling->right) && rb_is_black(sibling->left)) {
                rb_set_color(sibling, RB_RED);
                node = parent;
                parent = rb_parent(node);
            } else {
                if (rb_is_black(sibling->left)) {
                    rb_set_color(sibling->right, RB_BLACK);
                    rb_set_color(sibling, RB_RED);
                    rb_rotate_left(root, sibling);
                    sibling = parent->left;
                }
                
                rb_set_color(sibling, parent->color);
                rb_set_color(parent, RB_BLACK);
                rb_set_color(sibling->left, RB_BLACK);
                rb_rotate_right(root, parent);
                node = *root;
                break;
            }
        }
    }
    
    rb_set_color(node, RB_BLACK);
}

/* Delete node from red-black tree */
void ct_rb_delete(ct_rb_node_t **root, ct_rb_node_t *node) {
    ct_rb_node_t *child, *parent;
    int color;
    
    if (!node->left) {
        /* Node has at most one child (right) */
        child = node->right;
    } else if (!node->right) {
        /* Node has exactly one child (left) */
        child = node->left;
    } else {
        /* Node has two children - replace with successor */
        ct_rb_node_t *successor = rb_successor(node);
        
        /* Swap node with successor */
        if (rb_parent(successor) != node) {
            /* Successor is not direct child */
            if (successor->right) {
                successor->right->parent = rb_parent(successor);
            }
            rb_parent(successor)->left = successor->right;
            successor->right = node->right;
            node->right->parent = successor;
        }
        
        parent = rb_parent(node);
        color = successor->color;
        child = successor->right;
        
        if (parent) {
            if (parent->left == node) {
                parent->left = successor;
            } else {
                parent->right = successor;
            }
        } else {
            *root = successor;
        }
        
        successor->parent = rb_parent(node);
        successor->color = node->color;
        successor->left = node->left;
        node->left->parent = successor;
        
        if (color == RB_BLACK) {
            rb_delete_fixup(root, child, rb_parent(successor));
        }
        
        return;
    }
    
    parent = rb_parent(node);
    color = node->color;
    
    if (child) {
        child->parent = parent;
    }
    
    if (parent) {
        if (parent->left == node) {
            parent->left = child;
        } else {
            parent->right = child;
        }
    } else {
        *root = child;
    }
    
    if (color == RB_BLACK && child) {
        rb_delete_fixup(root, child, parent);
    }
}