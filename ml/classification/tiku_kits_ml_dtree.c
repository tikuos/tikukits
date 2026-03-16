/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ml_dtree.c - Binary decision tree classifier
 *
 * Platform-independent implementation of a pre-built decision tree
 * classifier. All computation uses integer comparison. No heap
 * allocation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "tiku_kits_ml_dtree.h"

/*---------------------------------------------------------------------------*/
/* INTERNAL HELPERS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Check whether a node is a leaf (has no children)
 *
 * A node is a leaf when both child indices equal the sentinel
 * TIKU_KITS_ML_DTREE_LEAF (0xFF).  This convention avoids needing
 * a separate node-type flag.
 */
static int is_leaf(const struct tiku_kits_ml_dtree_node *node)
{
    /* Both children must be the sentinel for a leaf */
    return (node->left == TIKU_KITS_ML_DTREE_LEAF
            && node->right == TIKU_KITS_ML_DTREE_LEAF);
}

/**
 * @brief Traverse the tree from root to leaf and return the leaf index
 *
 * Walks from node 0 downward, branching left or right at each
 * internal node based on the axis-aligned split condition
 * x[feature_index] <= threshold.  A loop guard limits iterations
 * to n_nodes to prevent infinite loops in malformed trees where
 * child indices form a cycle.
 *
 * O(depth) time, O(1) extra space.
 */
static int traverse(const struct tiku_kits_ml_dtree *dt,
                    const tiku_kits_ml_elem_t *x)
{
    uint8_t idx = 0;
    const struct tiku_kits_ml_dtree_node *node;
    uint8_t guard;

    /* Guard counter bounds the walk to at most n_nodes steps,
     * which is the maximum possible depth of any tree with
     * n_nodes nodes. */
    for (guard = 0; guard < dt->n_nodes; guard++) {
        node = &dt->nodes[idx];

        if (is_leaf(node)) {
            return (int)idx;
        }

        /* Axis-aligned split: go left when feature <= threshold */
        if (x[node->feature_index] <= node->threshold) {
            idx = node->left;
        } else {
            idx = node->right;
        }

        /* Bounds-check the child index against the loaded tree */
        if (idx >= dt->n_nodes) {
            return -1;
        }
    }

    return -1;  /* Exhausted guard -- likely a cyclic tree */
}

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a decision tree model
 *
 * Zeros the entire node buffer so that uninitialised reads return
 * deterministic values, validates the feature count and shift
 * parameters, and sets n_nodes to 0 (no tree loaded yet).
 */
int tiku_kits_ml_dtree_init(struct tiku_kits_ml_dtree *dt,
                             uint8_t n_features,
                             uint8_t shift)
{
    if (dt == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }
    if (n_features == 0
        || n_features > TIKU_KITS_ML_DTREE_MAX_FEATURES) {
        return TIKU_KITS_ML_ERR_PARAM;
    }
    if (shift < 1 || shift > 30) {
        return TIKU_KITS_ML_ERR_PARAM;
    }

    /* Zero the full static buffer so the struct is in a clean state
     * regardless of prior contents. */
    memset(dt->nodes, 0, sizeof(dt->nodes));
    dt->n_nodes = 0;
    dt->n_features = n_features;
    dt->shift = shift;

    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Reset the tree, clearing all loaded nodes
 *
 * Zeros the node buffer and resets n_nodes to 0 while preserving
 * the configured n_features and shift values.
 */
int tiku_kits_ml_dtree_reset(struct tiku_kits_ml_dtree *dt)
{
    if (dt == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }

    memset(dt->nodes, 0, sizeof(dt->nodes));
    dt->n_nodes = 0;

    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/
/* TREE LOADING                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief Load a pre-built decision tree from a node descriptor array
 *
 * Performs a full validation pass before copying: for every internal
 * node, child indices must point to valid nodes (< n_nodes) or be
 * the LEAF sentinel, and the feature_index must be within the
 * configured n_features.  Leaf nodes are skipped during validation
 * because their feature_index field is unused.
 *
 * O(n_nodes) validation + O(n_nodes) memcpy.
 */
int tiku_kits_ml_dtree_set_tree(
    struct tiku_kits_ml_dtree *dt,
    const struct tiku_kits_ml_dtree_node *nodes,
    uint8_t n_nodes)
{
    uint8_t i;
    const struct tiku_kits_ml_dtree_node *node;

    if (dt == NULL || nodes == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }
    if (n_nodes == 0 || n_nodes > TIKU_KITS_ML_DTREE_MAX_NODES) {
        return TIKU_KITS_ML_ERR_PARAM;
    }

    /* Validate every node before committing the copy so that
     * a partially-valid tree never overwrites the model. */
    for (i = 0; i < n_nodes; i++) {
        node = &nodes[i];

        if (is_leaf(node)) {
            continue;  /* Leaf nodes have no children to validate */
        }

        /* Internal node: child must be a valid index or sentinel */
        if (node->left != TIKU_KITS_ML_DTREE_LEAF
            && node->left >= n_nodes) {
            return TIKU_KITS_ML_ERR_PARAM;
        }
        if (node->right != TIKU_KITS_ML_DTREE_LEAF
            && node->right >= n_nodes) {
            return TIKU_KITS_ML_ERR_PARAM;
        }

        /* Feature index must be within the model's dimensionality */
        if (node->feature_index >= dt->n_features) {
            return TIKU_KITS_ML_ERR_PARAM;
        }
    }

    /* Copy only the valid nodes, not the full MAX_NODES buffer */
    memcpy(dt->nodes, nodes,
           (size_t)n_nodes * sizeof(struct tiku_kits_ml_dtree_node));
    dt->n_nodes = n_nodes;

    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Read back the loaded tree into a caller-supplied buffer
 *
 * Copies exactly n_nodes entries from the model's internal array
 * and writes the count to *n_nodes.  The caller must ensure the
 * output buffer has space for at least MAX_NODES entries.
 */
int tiku_kits_ml_dtree_get_tree(
    const struct tiku_kits_ml_dtree *dt,
    struct tiku_kits_ml_dtree_node *nodes,
    uint8_t *n_nodes)
{
    if (dt == NULL || nodes == NULL || n_nodes == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }

    memcpy(nodes, dt->nodes,
           (size_t)dt->n_nodes * sizeof(struct tiku_kits_ml_dtree_node));
    *n_nodes = dt->n_nodes;

    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/
/* PREDICTION                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Classify a feature vector by traversing the decision tree
 *
 * Delegates to traverse() and extracts the class_label from the
 * reached leaf node.  Returns ERR_SIZE if no tree has been loaded,
 * and ERR_PARAM if the traversal fails (malformed tree).
 */
int tiku_kits_ml_dtree_predict(
    const struct tiku_kits_ml_dtree *dt,
    const tiku_kits_ml_elem_t *x,
    uint8_t *result)
{
    int leaf_idx;

    if (dt == NULL || x == NULL || result == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }
    if (dt->n_nodes == 0) {
        return TIKU_KITS_ML_ERR_SIZE;
    }

    leaf_idx = traverse(dt, x);
    if (leaf_idx < 0) {
        return TIKU_KITS_ML_ERR_PARAM;
    }

    *result = dt->nodes[leaf_idx].class_label;
    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Get the confidence of the prediction as fixed-point
 *
 * Traverses the tree identically to predict() but reads the leaf's
 * threshold field as a Q(shift) confidence value rather than the
 * class_label.  A threshold of 0 is treated as "no confidence
 * stored" and full confidence (1 << shift) is returned as a
 * sensible default.
 */
int tiku_kits_ml_dtree_predict_proba(
    const struct tiku_kits_ml_dtree *dt,
    const tiku_kits_ml_elem_t *x,
    int32_t *result)
{
    int leaf_idx;
    int32_t conf;

    if (dt == NULL || x == NULL || result == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }
    if (dt->n_nodes == 0) {
        return TIKU_KITS_ML_ERR_SIZE;
    }

    leaf_idx = traverse(dt, x);
    if (leaf_idx < 0) {
        return TIKU_KITS_ML_ERR_PARAM;
    }

    conf = dt->nodes[leaf_idx].threshold;
    if (conf == 0) {
        /* No confidence was stored in this leaf; default to 1.0
         * in Q(shift) to indicate full certainty. */
        conf = (int32_t)(1 << dt->shift);
    }

    *result = conf;
    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the number of nodes in the loaded tree
 *
 * Safe to call with a NULL pointer -- returns 0 rather than
 * dereferencing.  Returns the loaded node count, not the
 * compile-time MAX_NODES capacity.
 */
uint8_t tiku_kits_ml_dtree_node_count(
    const struct tiku_kits_ml_dtree *dt)
{
    if (dt == NULL) {
        return 0;
    }
    return dt->n_nodes;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Get the maximum depth of the loaded tree
 *
 * Uses an explicit stack-based iterative DFS to avoid recursion
 * (important on stack-constrained embedded targets).  Each stack
 * frame stores a node index and its depth.  The right child is
 * pushed before the left so that left is visited first (matches
 * natural DFS order), though for depth measurement the visit
 * order is irrelevant -- we just need the maximum.
 *
 * O(n_nodes) time, O(n_nodes) stack space (two uint8_t arrays).
 */
uint8_t tiku_kits_ml_dtree_depth(
    const struct tiku_kits_ml_dtree *dt)
{
    uint8_t max_depth = 0;
    uint8_t stack_idx[TIKU_KITS_ML_DTREE_MAX_NODES];
    uint8_t stack_dep[TIKU_KITS_ML_DTREE_MAX_NODES];
    uint8_t sp = 0;
    uint8_t idx;
    uint8_t d;
    const struct tiku_kits_ml_dtree_node *n;

    if (dt == NULL || dt->n_nodes == 0) {
        return 0;
    }

    /* Seed the stack with the root at depth 1 */
    stack_idx[0] = 0;
    stack_dep[0] = 1;
    sp = 1;

    while (sp > 0) {
        sp--;
        idx = stack_idx[sp];
        d = stack_dep[sp];
        n = &dt->nodes[idx];

        if (d > max_depth) {
            max_depth = d;
        }

        /* Push right child first so left is popped (visited) first */
        if (n->right != TIKU_KITS_ML_DTREE_LEAF
            && n->right < dt->n_nodes && sp < TIKU_KITS_ML_DTREE_MAX_NODES) {
            stack_idx[sp] = n->right;
            stack_dep[sp] = d + 1;
            sp++;
        }
        if (n->left != TIKU_KITS_ML_DTREE_LEAF
            && n->left < dt->n_nodes && sp < TIKU_KITS_ML_DTREE_MAX_NODES) {
            stack_idx[sp] = n->left;
            stack_dep[sp] = d + 1;
            sp++;
        }
    }

    return max_depth;
}
