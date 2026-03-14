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
 * @brief Check if a node is a leaf
 * @param node Pointer to node
 * @return 1 if leaf, 0 if internal
 */
static int is_leaf(const struct tiku_kits_ml_dtree_node *node)
{
    return (node->left == TIKU_KITS_ML_DTREE_LEAF
            && node->right == TIKU_KITS_ML_DTREE_LEAF);
}

/**
 * @brief Traverse the tree and return the leaf node index
 * @param dt Model with loaded tree
 * @param x  Feature vector
 * @return Leaf node index, or -1 on error
 */
static int traverse(const struct tiku_kits_ml_dtree *dt,
                    const tiku_kits_ml_elem_t *x)
{
    uint8_t idx = 0;
    const struct tiku_kits_ml_dtree_node *node;
    uint8_t guard;

    /* Guard against infinite loops in malformed trees */
    for (guard = 0; guard < dt->n_nodes; guard++) {
        node = &dt->nodes[idx];

        if (is_leaf(node)) {
            return (int)idx;
        }

        if (x[node->feature_index] <= node->threshold) {
            idx = node->left;
        } else {
            idx = node->right;
        }

        if (idx >= dt->n_nodes) {
            return -1;
        }
    }

    return -1;
}

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

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

    memset(dt->nodes, 0, sizeof(dt->nodes));
    dt->n_nodes = 0;
    dt->n_features = n_features;
    dt->shift = shift;

    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/

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

    /* Validate node structure */
    for (i = 0; i < n_nodes; i++) {
        node = &nodes[i];

        if (is_leaf(node)) {
            continue;
        }

        /* Internal node: validate child indices */
        if (node->left != TIKU_KITS_ML_DTREE_LEAF
            && node->left >= n_nodes) {
            return TIKU_KITS_ML_ERR_PARAM;
        }
        if (node->right != TIKU_KITS_ML_DTREE_LEAF
            && node->right >= n_nodes) {
            return TIKU_KITS_ML_ERR_PARAM;
        }

        /* Validate feature index */
        if (node->feature_index >= dt->n_features) {
            return TIKU_KITS_ML_ERR_PARAM;
        }
    }

    /* Copy tree */
    memcpy(dt->nodes, nodes,
           (size_t)n_nodes * sizeof(struct tiku_kits_ml_dtree_node));
    dt->n_nodes = n_nodes;

    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/

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
        /* No confidence stored: assume full confidence */
        conf = (int32_t)(1 << dt->shift);
    }

    *result = conf;
    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

uint8_t tiku_kits_ml_dtree_node_count(
    const struct tiku_kits_ml_dtree *dt)
{
    if (dt == NULL) {
        return 0;
    }
    return dt->n_nodes;
}

/*---------------------------------------------------------------------------*/

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
