/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ml_dtree.h - Binary decision tree classifier
 *
 * Platform-independent pre-built decision tree classifier for embedded
 * systems. Supports multi-class classification using axis-aligned splits
 * on integer features. No FPU required.
 *
 * Trees are loaded via tiku_kits_ml_dtree_set_tree() from an array of
 * node descriptors. Each internal node stores a feature index and
 * threshold: go left if x[feature] <= threshold, else right. Leaf
 * nodes store the predicted class label and an optional confidence
 * value.
 *
 * Nodes use explicit left/right child indices (uint8_t) to support
 * unbalanced trees without wasting array slots. A sentinel value of
 * 0xFF marks "no child" (leaf). Maximum 255 nodes per tree.
 *
 * All storage is statically allocated; no heap required.
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

#ifndef TIKU_KITS_ML_DTREE_H_
#define TIKU_KITS_ML_DTREE_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_ml.h"

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * Element type for feature values.
 * Defaults to int32_t for integer-only targets (no FPU).
 * Must match the type defined in sibling ML modules.
 */
#ifndef TIKU_KITS_ML_ELEM_TYPE
#define TIKU_KITS_ML_ELEM_TYPE int32_t
#endif

/**
 * Maximum number of nodes in a decision tree.
 * 31 nodes allows a complete binary tree of depth 4, or deeper
 * unbalanced trees. Override before including to change the limit.
 */
#ifndef TIKU_KITS_ML_DTREE_MAX_NODES
#define TIKU_KITS_ML_DTREE_MAX_NODES 31
#endif

/**
 * Maximum number of input features.
 * Override before including this header to change the limit.
 */
#ifndef TIKU_KITS_ML_DTREE_MAX_FEATURES
#define TIKU_KITS_ML_DTREE_MAX_FEATURES 8
#endif

/**
 * Default number of fractional bits for fixed-point confidence
 * outputs. With shift=8, the resolution is ~0.004.
 * Override before including this header to change the default.
 */
#ifndef TIKU_KITS_ML_DTREE_SHIFT
#define TIKU_KITS_ML_DTREE_SHIFT 8
#endif

/** Sentinel value indicating a leaf node (no child) */
#define TIKU_KITS_ML_DTREE_LEAF 0xFF

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/** @typedef tiku_kits_ml_elem_t
 *  @brief Element type used for all feature values
 */
typedef TIKU_KITS_ML_ELEM_TYPE tiku_kits_ml_elem_t;

/**
 * @struct tiku_kits_ml_dtree_node
 * @brief Single node in a decision tree
 *
 * Internal nodes split on x[feature_index] <= threshold:
 *   - left child taken if condition is true
 *   - right child taken otherwise
 *
 * Leaf nodes are identified by left == right == TIKU_KITS_ML_DTREE_LEAF.
 * For leaf nodes, class_label holds the predicted class and threshold
 * may optionally hold a confidence value in Q(shift) format.
 */
struct tiku_kits_ml_dtree_node {
    int32_t threshold;      /**< Split threshold, or leaf confidence */
    uint8_t feature_index;  /**< Feature to split on (internal only) */
    uint8_t left;           /**< Left child index, or LEAF sentinel */
    uint8_t right;          /**< Right child index, or LEAF sentinel */
    uint8_t class_label;    /**< Predicted class (leaf nodes) */
};

/**
 * @struct tiku_kits_ml_dtree
 * @brief Pre-built decision tree classifier
 *
 * Stores a tree as a flat array of nodes with explicit child indices.
 * Node 0 is always the root. Trees are loaded via set_tree() and
 * classification is performed via predict().
 *
 * Example:
 * @code
 *   struct tiku_kits_ml_dtree dt;
 *   uint8_t cls;
 *
 *   tiku_kits_ml_dtree_init(&dt, 1, 8);
 *
 *   // Tree: x[0] <= 5 -> class 0, else class 1
 *   struct tiku_kits_ml_dtree_node nodes[] = {
 *       {5, 0, 1, 2, 0},              // root: split on feature 0
 *       {0, 0, 0xFF, 0xFF, 0},        // leaf: class 0
 *       {0, 0, 0xFF, 0xFF, 1},        // leaf: class 1
 *   };
 *   tiku_kits_ml_dtree_set_tree(&dt, nodes, 3);
 *
 *   tiku_kits_ml_elem_t x[] = {3};
 *   tiku_kits_ml_dtree_predict(&dt, x, &cls);  // cls = 0
 * @endcode
 */
struct tiku_kits_ml_dtree {
    struct tiku_kits_ml_dtree_node
        nodes[TIKU_KITS_ML_DTREE_MAX_NODES];
        /**< Node array: [0] is root */
    uint8_t n_nodes;       /**< Number of nodes loaded */
    uint8_t n_features;    /**< Number of input features */
    uint8_t shift;         /**< Fixed-point fractional bits */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a decision tree model
 * @param dt         Model to initialize
 * @param n_features Number of input features (1..MAX_FEATURES)
 * @param shift      Number of fixed-point fractional bits (1..30)
 * @return TIKU_KITS_ML_OK, TIKU_KITS_ML_ERR_NULL, or
 *         TIKU_KITS_ML_ERR_PARAM (invalid n_features or shift)
 *
 * All nodes are zeroed and n_nodes is set to 0. A tree must be
 * loaded via set_tree() before prediction is possible.
 */
int tiku_kits_ml_dtree_init(struct tiku_kits_ml_dtree *dt,
                             uint8_t n_features,
                             uint8_t shift);

/**
 * @brief Reset the tree, clearing all nodes
 * @param dt Model to reset
 * @return TIKU_KITS_ML_OK or TIKU_KITS_ML_ERR_NULL
 *
 * Preserves n_features and shift.
 */
int tiku_kits_ml_dtree_reset(struct tiku_kits_ml_dtree *dt);

/*---------------------------------------------------------------------------*/
/* TREE LOADING                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief Load a pre-built decision tree
 * @param dt      Model (must be initialized)
 * @param nodes   Array of node descriptors (node 0 = root)
 * @param n_nodes Number of nodes (1..MAX_NODES)
 * @return TIKU_KITS_ML_OK, TIKU_KITS_ML_ERR_NULL, or
 *         TIKU_KITS_ML_ERR_PARAM (invalid n_nodes, child indices
 *         out of range, or feature index out of range)
 *
 * The tree is validated during loading: child indices must be
 * < n_nodes or TIKU_KITS_ML_DTREE_LEAF, and feature indices of
 * internal nodes must be < n_features.
 */
int tiku_kits_ml_dtree_set_tree(
    struct tiku_kits_ml_dtree *dt,
    const struct tiku_kits_ml_dtree_node *nodes,
    uint8_t n_nodes);

/**
 * @brief Read back the loaded tree
 * @param dt      Model
 * @param nodes   Output array (caller must provide MAX_NODES entries)
 * @param n_nodes Output: number of nodes
 * @return TIKU_KITS_ML_OK or TIKU_KITS_ML_ERR_NULL
 */
int tiku_kits_ml_dtree_get_tree(
    const struct tiku_kits_ml_dtree *dt,
    struct tiku_kits_ml_dtree_node *nodes,
    uint8_t *n_nodes);

/*---------------------------------------------------------------------------*/
/* PREDICTION                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Classify a feature vector
 * @param dt     Model (must have a tree loaded)
 * @param x      Feature vector of length n_features
 * @param result Output: predicted class label
 * @return TIKU_KITS_ML_OK, TIKU_KITS_ML_ERR_NULL, or
 *         TIKU_KITS_ML_ERR_SIZE (no tree loaded)
 *
 * Traverses the tree from the root. At each internal node, goes
 * left if x[feature_index] <= threshold, otherwise goes right.
 * Returns the class_label of the reached leaf node.
 */
int tiku_kits_ml_dtree_predict(
    const struct tiku_kits_ml_dtree *dt,
    const tiku_kits_ml_elem_t *x,
    uint8_t *result);

/**
 * @brief Get the confidence of the prediction as fixed-point
 * @param dt     Model (must have a tree loaded)
 * @param x      Feature vector of length n_features
 * @param result Output: confidence * (1 << shift), range [0, 1<<shift]
 * @return TIKU_KITS_ML_OK, TIKU_KITS_ML_ERR_NULL, or
 *         TIKU_KITS_ML_ERR_SIZE (no tree loaded)
 *
 * Traverses the tree and returns the threshold field of the leaf
 * node, which should contain a confidence value in Q(shift) format.
 * If the leaf's threshold is 0, returns (1 << shift) (full confidence).
 */
int tiku_kits_ml_dtree_predict_proba(
    const struct tiku_kits_ml_dtree *dt,
    const tiku_kits_ml_elem_t *x,
    int32_t *result);

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the number of nodes in the loaded tree
 * @param dt Model
 * @return Number of nodes, or 0 if dt is NULL or no tree loaded
 */
uint8_t tiku_kits_ml_dtree_node_count(
    const struct tiku_kits_ml_dtree *dt);

/**
 * @brief Get the depth of the loaded tree
 * @param dt Model
 * @return Maximum depth (root = depth 1), or 0 if empty/NULL
 */
uint8_t tiku_kits_ml_dtree_depth(
    const struct tiku_kits_ml_dtree *dt);

#endif /* TIKU_KITS_ML_DTREE_H_ */
