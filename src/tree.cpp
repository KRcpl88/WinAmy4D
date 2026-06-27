/*

    Amy - a chess playing program

    Copyright (c) 2002-2026, Thorsten Greiner
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.

*/

#include "amy.h"


#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "safe_malloc.h"
#include "tree.h"

/** Magic constant to identify trees written to disk. */
static const char *MAGIC = "ATRE";

/**
 * Allocate a tree node.
 */
static tree_node_t *allocate_node(void *pKeyData, size_t qwKeyLen,
                                  void *pValueData, size_t qwValueLen) {
    tree_node_t *node = (tree_node_t *)safe_malloc(sizeof(tree_node_t));

    node->key_data = (char *)safe_malloc(qwKeyLen);
    node->key_len = qwKeyLen;

    node->value_data = safe_malloc(qwValueLen);
    node->value_len = qwValueLen;

    memcpy(node->key_data, pKeyData, qwKeyLen);
    memcpy(node->value_data, pValueData, qwValueLen);

    node->left_child = NULL;
    node->right_child = NULL;
    node->depth = 0;

    return node;
}

/**
 * Free a tree node recursively.
 */
void free_node(tree_node_t *node) {
    if (node == NULL)
        return;

    free_node(node->left_child);
    free_node(node->right_child);

    free(node->value_data);
    free(node->key_data);
    free(node);
}

/**
 * Compare two keys.
 */
static int cmp_keys(const char *pKey1, size_t qwLen1, const char *pKey2,
                    size_t qwLen2) {
    size_t qwMinLen = (qwLen1 < qwLen2) ? qwLen1 : qwLen2;
    int nResult = memcmp(pKey1, pKey2, qwMinLen);

    if (nResult != 0) {
        return nResult;
    }

    return (int)(qwLen1 - qwLen2);
}

/**
 * Get the depth of a node - returns 0 if node is NULL.
 */
static inline unsigned int get_depth(tree_node_t *node) {
    return (node == NULL) ? 0 : node->depth;
}

/**
 * Checks whether node is a leaf node (has no children).
 */
static inline bool is_leaf(tree_node_t *node) {
    return node->left_child == NULL && node->right_child == NULL;
}

/**
 * Update 'depth' of the supplied node using the depth information of
 * its children.
 */
static void update_depth(tree_node_t *node) {
    if (is_leaf(node)) {
        node->depth = 0;
        return;
    }
    unsigned int dwLeftDepth = get_depth(node->left_child);
    unsigned int dwRightDepth = get_depth(node->right_child);
    unsigned int dwMaxDepth =
        (dwLeftDepth > dwRightDepth) ? dwLeftDepth : dwRightDepth;
    node->depth = dwMaxDepth + 1;
}

/**
 * Perform a right rotation of node.
 */
static tree_node_t *rotate_right(tree_node_t *node) {
    tree_node_t *child = node->left_child;
    tree_node_t *pTmp = child->right_child;

    child->right_child = node;
    node->left_child = pTmp;

    update_depth(node);
    update_depth(child);

    return child;
}

/**
 * Perform a left rotation of a node.
 */
static tree_node_t *rotate_left(tree_node_t *node) {
    tree_node_t *child = node->right_child;
    tree_node_t *pTmp = child->left_child;

    child->left_child = node;
    node->right_child = pTmp;

    update_depth(node);
    update_depth(child);

    return child;
}

/**
 * Performs a right rotation or a left right rotation of node.
 */
static tree_node_t *rotate_right_full(tree_node_t *node) {
    tree_node_t *child = node->left_child;
    if (get_depth(child->right_child) > get_depth(child->left_child)) {
        child = rotate_left(child);
        node->left_child = child;
    }
    return rotate_right(node);
}

/**
 * Performs a left rotation or a right left rotation of node.
 */
static tree_node_t *rotate_left_full(tree_node_t *node) {
    tree_node_t *child = node->right_child;
    if (get_depth(child->left_child) > get_depth(child->right_child)) {
        child = rotate_right(child);
        node->right_child = child;
    }
    return rotate_left(node);
}

/**
 * Balance a node.
 */
tree_node_t *balance(tree_node_t *node) {
    unsigned int dwLeftDepth = get_depth(node->left_child);
    unsigned int dwRightDepth = get_depth(node->right_child);

    int nImbalance = dwLeftDepth - dwRightDepth;

    if (abs(nImbalance) < 2) {
        return node;
    }

    if (nImbalance > 0) {
        return rotate_right_full(node);
    } else {
        return rotate_left_full(node);
    }
}

/**
 * Add a node to the tree.
 */
tree_node_t *add_node(tree_node_t *node, void *pKeyData, size_t qwKeyLen,
                      void *pValueData, size_t qwValueLen) {
    if (node == NULL) {
        return allocate_node(pKeyData, qwKeyLen, pValueData, qwValueLen);
    }

    int comparison =
        cmp_keys((const char *)pKeyData, qwKeyLen, node->key_data, node->key_len);

    if (comparison == 0) {
        node->value_data = safe_realloc(node->value_data, qwValueLen);
        if (node->value_data == NULL) {
            perror("Failed to allocate value_data");
            exit(1);
        }
        memcpy(node->value_data, pValueData, qwValueLen);
        return node;
    } else if (comparison < 0) {
        node->left_child = add_node(node->left_child, pKeyData, qwKeyLen,
                                    pValueData, qwValueLen);
    } else {
        node->right_child = add_node(node->right_child, pKeyData, qwKeyLen,
                                     pValueData, qwValueLen);
    }
    update_depth(node);

    return balance(node);
}

/**
 * Lookup a value in the tree.
 */
static void *lookup_value_internal(tree_node_t *node, const char *pKeyData,
                                   size_t qwKeyLen, size_t *pValueLen,
                                   int depth) {
    if (node == NULL) {
        return NULL;
    }

    int comparison = cmp_keys(pKeyData, qwKeyLen, node->key_data, node->key_len);

    if (comparison == 0) {
        if (pValueLen != NULL) {
            *pValueLen = node->value_len;
        }
        char *buffer = (char *)safe_malloc(node->value_len);
        memcpy(buffer, node->value_data, node->value_len);
        return buffer;
    } else if (comparison < 0) {
        return lookup_value_internal(node->left_child, pKeyData, qwKeyLen,
                                     pValueLen, depth + 1);
    } else {
        return lookup_value_internal(node->right_child, pKeyData, qwKeyLen,
                                     pValueLen, depth + 1);
    }
}

/**
 * Lookup a value in the tree. Returns NULL if the tree does not
 * contain the key. Otherwise, a copy of the value is returned.
 * Use free() on the return value to free the memory of the copy.
 */
void *lookup_value(tree_node_t *node, const void *pKeyData, size_t qwKeyLen,
                   size_t *pValueLen) {
    return lookup_value_internal(node, (const char *)pKeyData, qwKeyLen,
                                 pValueLen, 0);
}

/**
 * Write a size_t value to fout. This does a little bit of compression
 * by saving only 7 bits of the value and setting a continuation
 * bit if the value exceeds seven bits.
 */
static inline void write_size(size_t qwValue, FILE *fout) {
    for (;;) {
        int nOutputValue = qwValue & 0x7f;
        qwValue >>= 7;
        if (qwValue) {
            nOutputValue |= 0x80;
        }
        fputc(nOutputValue, fout);

        if (qwValue == 0)
            break;
    }
}

/**
 * Traverse the tree recursively and write to file.
 */
static void save_tree_recursive(tree_node_t *node, FILE *fout) {
    if (node == NULL) {
        return;
    }

    write_size(node->key_len, fout);
    fwrite(node->key_data, node->key_len, 1, fout);
    write_size(node->value_len, fout);
    fwrite(node->value_data, node->value_len, 1, fout);

    save_tree_recursive(node->left_child, fout);
    save_tree_recursive(node->right_child, fout);
}

/**
 * Save the tree to a file.
 */
void save_tree(tree_node_t *node, FILE *fout) {
    size_t qwRecordsWritten = fwrite(MAGIC, 4, 1, fout);
    if (qwRecordsWritten != 1)
        return;
    save_tree_recursive(node, fout);
}

/**
 * Read a size_t value from fin. This uncompresses the
 * value written by write_size.
 */
static inline size_t read_size(FILE *fin) {
    size_t qwValue = 0;
    for (;;) {
        int nInputValue = fgetc(fin);
        if (nInputValue == EOF)
            return 0;

        qwValue = (qwValue << 7) | (nInputValue & 0x7f);
        if ((nInputValue & 0x80) == 0)
            break;
    }
    return qwValue;
}

/**
 * Load the tree from a file.
 */
static tree_node_t *load_tree_internal(FILE *fin) {
    tree_node_t *node = NULL;
    size_t qwKeyLen;
    size_t qwValueLen;
    char *pKeyData = (char *)safe_malloc(8);
    char *pValueData = (char *)safe_malloc(256);

    for (;;) {
        qwKeyLen = read_size(fin);
        if (qwKeyLen == 0)
            break;

        pKeyData = (char *)safe_realloc(pKeyData, qwKeyLen);
        size_t qwAmountRead = fread(pKeyData, qwKeyLen, 1, fin);
        if (qwAmountRead != 1)
            break;

        qwValueLen = read_size(fin);
        if (qwValueLen == 0)
            break;

        pValueData = (char *)safe_realloc(pValueData, qwValueLen);
        qwAmountRead = fread(pValueData, qwValueLen, 1, fin);
        if (qwAmountRead != 1)
            break;

        node = add_node(node, pKeyData, qwKeyLen, pValueData, qwValueLen);
    }

    free(pKeyData);
    free(pValueData);

    return node;
}

/**
 * Load the tree from a file.
 */
tree_node_t *load_tree(FILE *fin) {
    char buffer[4];
    size_t qwRecordsRead = fread(buffer, 4, 1, fin);
    if (qwRecordsRead != 1)
        return NULL;

    if (memcmp(buffer, MAGIC, 4) != 0)
        return NULL;

    return load_tree_internal(fin);
}
