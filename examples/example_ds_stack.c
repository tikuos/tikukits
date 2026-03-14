/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * TikuKits Example: Stack Data Structure
 *
 * Demonstrates the fixed-size LIFO stack for embedded applications:
 *   - Expression matching (balanced brackets)
 *   - Undo/redo operation history
 *   - DFS traversal of a small graph
 *
 * All stacks use static storage (no heap). Maximum capacity is
 * controlled by TIKU_KITS_DS_STACK_MAX_SIZE (default 16).
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

#include <stdio.h>
#include <stdint.h>

#include "../ds/tiku_kits_ds.h"
#include "../ds/stack/tiku_kits_ds_stack.h"

/*---------------------------------------------------------------------------*/
/* Example 1: Balanced bracket checker                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Check if a string of brackets is balanced
 *
 * Uses the stack to track opening brackets and match them with
 * closing brackets. Supports (), [], {}.
 */
static int check_balanced(const char *expr)
{
    struct tiku_kits_ds_stack stk;
    tiku_kits_ds_elem_t top;
    const char *p;

    tiku_kits_ds_stack_init(&stk, 16);

    for (p = expr; *p != '\0'; p++) {
        if (*p == '(' || *p == '[' || *p == '{') {
            if (tiku_kits_ds_stack_push(&stk,
                    (tiku_kits_ds_elem_t)*p)
                != TIKU_KITS_DS_OK) {
                return 0;  /* Stack overflow -- too deeply nested */
            }
        } else if (*p == ')' || *p == ']' || *p == '}') {
            if (tiku_kits_ds_stack_pop(&stk, &top)
                != TIKU_KITS_DS_OK) {
                return 0;  /* Unmatched closing bracket */
            }
            if ((*p == ')' && top != '(') ||
                (*p == ']' && top != '[') ||
                (*p == '}' && top != '{')) {
                return 0;  /* Mismatched pair */
            }
        }
    }

    return tiku_kits_ds_stack_empty(&stk);
}

static void example_bracket_matching(void)
{
    static const char *exprs[] = {
        "{[()]}",
        "((()))",
        "{[}]",
        "(()",
        ""
    };
    uint16_t i;

    printf("--- Balanced Bracket Checker ---\n");

    for (i = 0; i < 5; i++) {
        printf("  \"%s\" -> %s\n",
               exprs[i],
               check_balanced(exprs[i]) ? "balanced" : "NOT balanced");
    }
}

/*---------------------------------------------------------------------------*/
/* Example 2: Undo/redo operation history                                    */
/*---------------------------------------------------------------------------*/

/**
 * Simple register machine: one accumulator, undo via stack.
 * Operations are encoded as signed offsets applied to the value.
 */
static void example_undo_redo(void)
{
    struct tiku_kits_ds_stack undo_stack;
    int32_t accumulator = 0;
    tiku_kits_ds_elem_t delta;

    /* Sequence of operations: +10, +5, -3 */
    static const int32_t ops[] = { 10, 5, -3 };
    uint16_t i;

    printf("--- Undo/Redo History ---\n");

    tiku_kits_ds_stack_init(&undo_stack, 16);

    /* Apply operations, recording deltas for undo */
    for (i = 0; i < 3; i++) {
        accumulator += ops[i];
        tiku_kits_ds_stack_push(&undo_stack,
                                (tiku_kits_ds_elem_t)ops[i]);
        printf("  Apply %+ld -> accumulator = %ld\n",
               (long)ops[i], (long)accumulator);
    }

    /* Undo last two operations */
    printf("  -- Undo --\n");
    for (i = 0; i < 2; i++) {
        tiku_kits_ds_stack_pop(&undo_stack, &delta);
        accumulator -= delta;
        printf("  Undo %+ld -> accumulator = %ld\n",
               (long)delta, (long)accumulator);
    }
    /* accumulator should be 10 (only first op remains) */
    printf("  Final accumulator: %ld\n", (long)accumulator);
}

/*---------------------------------------------------------------------------*/
/* Example 3: DFS traversal of a small graph                                 */
/*---------------------------------------------------------------------------*/

/**
 * Simple adjacency-list graph with 5 nodes (0..4).
 * Edges: 0->1, 0->2, 1->3, 2->3, 3->4
 */

#define NUM_NODES 5
#define MAX_EDGES 3

static void example_dfs_traversal(void)
{
    struct tiku_kits_ds_stack stk;
    tiku_kits_ds_elem_t node;
    int visited[NUM_NODES];
    uint16_t i;

    /*
     * adj[n][0] = number of neighbors
     * adj[n][1..] = neighbor indices
     */
    static const int32_t adj[NUM_NODES][MAX_EDGES + 1] = {
        { 2, 1, 2 },    /* Node 0 -> 1, 2    */
        { 1, 3, 0 },    /* Node 1 -> 3       */
        { 1, 3, 0 },    /* Node 2 -> 3       */
        { 1, 4, 0 },    /* Node 3 -> 4       */
        { 0, 0, 0 }     /* Node 4 -> (none)  */
    };

    printf("--- DFS Traversal ---\n");

    tiku_kits_ds_stack_init(&stk, 16);

    for (i = 0; i < NUM_NODES; i++) {
        visited[i] = 0;
    }

    /* Start DFS from node 0 */
    tiku_kits_ds_stack_push(&stk, 0);

    printf("  Visit order: ");
    while (tiku_kits_ds_stack_empty(&stk) == 0) {
        tiku_kits_ds_stack_pop(&stk, &node);

        if (visited[node]) {
            continue;
        }
        visited[node] = 1;
        printf("%ld ", (long)node);

        /* Push neighbors (reverse order for consistent DFS) */
        {
            int32_t n_neighbors = adj[node][0];
            int32_t j;
            for (j = n_neighbors; j >= 1; j--) {
                int32_t neighbor = adj[node][j];
                if (!visited[neighbor]) {
                    tiku_kits_ds_stack_push(
                        &stk,
                        (tiku_kits_ds_elem_t)neighbor);
                }
            }
        }
    }
    printf("\n");
}

/*---------------------------------------------------------------------------*/
/* Main                                                                      */
/*---------------------------------------------------------------------------*/

void example_ds_stack_run(void)
{
    printf("=== TikuKits Stack Examples ===\n\n");

    example_bracket_matching();
    printf("\n");

    example_undo_redo();
    printf("\n");

    example_dfs_traversal();
    printf("\n");
}
