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
 * @brief Element type for feature values.
 *
 * Defaults to int32_t for integer-only targets (no FPU required).
 * All ML sub-modules share this type so that feature vectors can be
 * passed between classifiers without casting.  Define as int16_t
 * before including if memory is very tight on a 16-bit target.
 *
 * Override before including this header to change the type:
 * @code
 *   #define TIKU_KITS_ML_ELEM_TYPE int16_t
 *   #include "tiku_kits_ml_dtree.h"
 * @endcode
 */
#ifndef TIKU_KITS_ML_ELEM_TYPE
#define TIKU_KITS_ML_ELEM_TYPE int32_t
#endif

/**
 * @brief Maximum number of nodes in a decision tree.
 *
 * This compile-time constant defines the upper bound on tree size.
 * Each model instance reserves this many node slots in its static
 * storage.  31 nodes allows a complete binary tree of depth 4, or
 * deeper unbalanced trees with fewer nodes.  Maximum 255 nodes
 * because child indices are uint8_t.
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_ML_DTREE_MAX_NODES 63
 *   #include "tiku_kits_ml_dtree.h"
 * @endcode
 */
#ifndef TIKU_KITS_ML_DTREE_MAX_NODES
#define TIKU_KITS_ML_DTREE_MAX_NODES 31
#endif

/**
 * @brief Maximum number of input features.
 *
 * Determines the maximum dimensionality of input feature vectors.
 * Each internal node references a feature by its index, which must
 * be less than this value.  Choose a value that covers your data
 * without wasting validation overhead.
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_ML_DTREE_MAX_FEATURES 16
 *   #include "tiku_kits_ml_dtree.h"
 * @endcode
 */
#ifndef TIKU_KITS_ML_DTREE_MAX_FEATURES
#define TIKU_KITS_ML_DTREE_MAX_FEATURES 8
#endif

/**
 * @brief Default number of fractional bits for fixed-point confidence.
 *
 * Controls the resolution of confidence values stored in leaf nodes.
 * With shift=8 the resolution is ~0.004 (1/256), which is adequate
 * for most embedded classification tasks.  Larger shift gives finer
 * resolution but reduces the representable integer range.
 *
 * Override before including this header to change the default:
 * @code
 *   #define TIKU_KITS_ML_DTREE_SHIFT 10
 *   #include "tiku_kits_ml_dtree.h"
 * @endcode
 */
#ifndef TIKU_KITS_ML_DTREE_SHIFT
#define TIKU_KITS_ML_DTREE_SHIFT 8
#endif

/**
 * @brief Sentinel value indicating a leaf node (no child).
 *
 * When both the @c left and @c right child indices of a node equal
 * this sentinel (0xFF), the node is a leaf.  The value 0xFF was
 * chosen because child indices are uint8_t, and a valid tree can
 * hold at most 255 nodes (indices 0..254), leaving 0xFF unused.
 */
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
 * @brief Single node in a binary decision tree
 *
 * A union-style record that serves double duty as either an internal
 * decision node or a leaf (prediction) node, distinguished by the
 * child-index sentinel pattern.
 *
 * **Internal nodes** split on @c x[feature_index] <= threshold:
 *   - @c left child is taken when the condition holds
 *   - @c right child is taken otherwise
 *
 * **Leaf nodes** are identified by
 *   @c left == right == TIKU_KITS_ML_DTREE_LEAF (0xFF).
 * For leaves, @c class_label holds the predicted class and
 * @c threshold may optionally hold a confidence value encoded in
 * Q(shift) fixed-point format.
 *
 * @note Nodes use explicit uint8_t child indices rather than
 *       implicit 2i+1 / 2i+2 positioning so that unbalanced trees
 *       do not waste array slots.  The trade-off is 2 extra bytes
 *       per node versus potentially many empty slots.
 */
struct tiku_kits_ml_dtree_node {
    int32_t threshold;      /**< Split threshold (internal), or
                                 confidence in Q(shift) (leaf) */
    uint8_t feature_index;  /**< Index into the feature vector to
                                 split on (meaningful for internal
                                 nodes only, must be < n_features) */
    uint8_t left;           /**< Left child index (0..n_nodes-1), or
                                 TIKU_KITS_ML_DTREE_LEAF for leaves */
    uint8_t right;          /**< Right child index (0..n_nodes-1), or
                                 TIKU_KITS_ML_DTREE_LEAF for leaves */
    uint8_t class_label;    /**< Predicted class (meaningful for leaf
                                 nodes; ignored for internal nodes) */
};

/**
 * @struct tiku_kits_ml_dtree
 * @brief Pre-built binary decision tree classifier
 *
 * Stores a tree as a flat array of node descriptors with explicit
 * child indices.  Node 0 is always the root.  Trees are trained
 * off-device (e.g. in Python) and loaded at run time via
 * set_tree().  Classification traverses from the root to a leaf
 * using only integer comparison -- no FPU required.
 *
 * Two parameters govern fixed-point behaviour:
 *   - @c n_features -- validated against each internal node's
 *     feature_index during set_tree() to reject malformed trees.
 *   - @c shift -- fractional bits used to interpret confidence
 *     values stored in leaf nodes' threshold field.
 *
 * @note Because all storage lives inside the struct, no heap
 *       allocation is needed -- just declare the model as a static
 *       or local variable.  The backing buffer always reserves
 *       TIKU_KITS_ML_DTREE_MAX_NODES slots regardless of the
 *       actual tree size.
 *
 * Example -- a 3-node tree with one split:
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
        /**< Flat node array; index 0 is always the root */
    uint8_t n_nodes;       /**< Number of valid nodes loaded via
                                set_tree() (0 means empty) */
    uint8_t n_features;    /**< Dimensionality of input feature
                                vectors (set at init time) */
    uint8_t shift;         /**< Fixed-point fractional bits for
                                interpreting leaf confidence values */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a decision tree model
 *
 * Resets the model to an empty state, zeros the entire node buffer,
 * and records the feature dimensionality and fixed-point shift.  A
 * tree must be loaded via set_tree() before prediction is possible.
 *
 * @param dt         Model to initialize (must not be NULL)
 * @param n_features Number of input features
 *                   (1..TIKU_KITS_ML_DTREE_MAX_FEATURES)
 * @param shift      Number of fixed-point fractional bits (1..30)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if dt is NULL,
 *         TIKU_KITS_ML_ERR_PARAM if n_features is 0 or exceeds
 *         MAX_FEATURES, or shift is out of range
 */
int tiku_kits_ml_dtree_init(struct tiku_kits_ml_dtree *dt,
                             uint8_t n_features,
                             uint8_t shift);

/**
 * @brief Reset the tree, clearing all loaded nodes
 *
 * Zeros the node buffer and sets n_nodes to 0 while preserving the
 * configured n_features and shift values.  Useful for reloading a
 * different tree into the same model instance without a full reinit.
 *
 * @param dt Model to reset (must not be NULL)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if dt is NULL
 */
int tiku_kits_ml_dtree_reset(struct tiku_kits_ml_dtree *dt);

/*---------------------------------------------------------------------------*/
/* TREE LOADING                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief Load a pre-built decision tree from a node descriptor array
 *
 * Copies the caller-supplied node array into the model's static
 * buffer and validates the tree structure during loading.  For
 * every internal node, child indices must be either a valid node
 * index (< n_nodes) or the TIKU_KITS_ML_DTREE_LEAF sentinel, and
 * the feature_index must be < n_features.  This ensures that
 * subsequent traversals cannot access out-of-bounds memory.
 *
 * O(n_nodes) -- one validation pass plus one memcpy.
 *
 * @param dt      Model (must be initialized via init; must not be NULL)
 * @param nodes   Array of node descriptors with node 0 as the root
 *                (must not be NULL)
 * @param n_nodes Number of nodes to load
 *                (1..TIKU_KITS_ML_DTREE_MAX_NODES)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if dt or nodes is NULL,
 *         TIKU_KITS_ML_ERR_PARAM if n_nodes is 0 or exceeds
 *         MAX_NODES, or any child / feature index is out of range
 */
int tiku_kits_ml_dtree_set_tree(
    struct tiku_kits_ml_dtree *dt,
    const struct tiku_kits_ml_dtree_node *nodes,
    uint8_t n_nodes);

/**
 * @brief Read back the loaded tree into a caller-supplied buffer
 *
 * Copies the model's internal node array and node count out to the
 * caller.  Useful for serialization or inspection of the loaded tree.
 *
 * @param dt      Model (must not be NULL)
 * @param nodes   Output array; caller must provide space for at least
 *                TIKU_KITS_ML_DTREE_MAX_NODES entries (must not be NULL)
 * @param n_nodes Output pointer where the number of valid nodes is
 *                written (must not be NULL)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if dt, nodes, or n_nodes is NULL
 */
int tiku_kits_ml_dtree_get_tree(
    const struct tiku_kits_ml_dtree *dt,
    struct tiku_kits_ml_dtree_node *nodes,
    uint8_t *n_nodes);

/*---------------------------------------------------------------------------*/
/* PREDICTION                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Classify a feature vector by traversing the decision tree
 *
 * Starting at the root (node 0), the algorithm walks down the tree.
 * At each internal node it compares @c x[feature_index] against the
 * node's threshold -- going left when the condition holds, right
 * otherwise.  When a leaf is reached, its class_label is returned.
 *
 * O(depth) -- bounded by n_nodes in the worst case.  A loop guard
 * prevents infinite traversal on malformed trees.
 *
 * @param dt     Model with a tree loaded via set_tree()
 *               (must not be NULL)
 * @param x      Feature vector of length n_features (must not be NULL)
 * @param result Output pointer where the predicted class label is
 *               written (must not be NULL)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if dt, x, or result is NULL,
 *         TIKU_KITS_ML_ERR_SIZE if no tree is loaded (n_nodes == 0),
 *         TIKU_KITS_ML_ERR_PARAM if traversal hits an invalid node
 */
int tiku_kits_ml_dtree_predict(
    const struct tiku_kits_ml_dtree *dt,
    const tiku_kits_ml_elem_t *x,
    uint8_t *result);

/**
 * @brief Get the confidence of the prediction as fixed-point
 *
 * Traverses the tree identically to predict() but returns the leaf
 * node's @c threshold field, which should contain a confidence value
 * encoded in Q(shift) fixed-point (range [0, 1 << shift]).  If the
 * leaf's threshold is 0 (no confidence stored), full confidence
 * (1 << shift) is returned as a default.
 *
 * O(depth) -- same traversal cost as predict().
 *
 * @param dt     Model with a tree loaded via set_tree()
 *               (must not be NULL)
 * @param x      Feature vector of length n_features (must not be NULL)
 * @param result Output pointer where the confidence value is written
 *               in Q(shift) fixed-point (must not be NULL)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if dt, x, or result is NULL,
 *         TIKU_KITS_ML_ERR_SIZE if no tree is loaded (n_nodes == 0),
 *         TIKU_KITS_ML_ERR_PARAM if traversal hits an invalid node
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
 *
 * Returns the count of nodes loaded via set_tree(), not the
 * compile-time MAX_NODES capacity.  Safe to call with a NULL
 * pointer -- returns 0.
 *
 * @param dt Model, or NULL
 * @return Number of loaded nodes, or 0 if dt is NULL or no tree
 *         has been loaded
 */
uint8_t tiku_kits_ml_dtree_node_count(
    const struct tiku_kits_ml_dtree *dt);

/**
 * @brief Get the maximum depth of the loaded tree
 *
 * Performs an iterative depth-first traversal using an explicit
 * stack to measure the longest root-to-leaf path.  Root is at
 * depth 1.  O(n_nodes) time, O(n_nodes) stack space.
 *
 * @param dt Model, or NULL
 * @return Maximum depth (root = 1), or 0 if dt is NULL or no tree
 *         has been loaded
 */
uint8_t tiku_kits_ml_dtree_depth(
    const struct tiku_kits_ml_dtree *dt);

#endif /* TIKU_KITS_ML_DTREE_H_ */
