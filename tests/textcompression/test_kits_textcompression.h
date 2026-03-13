/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * test_kits_textcompression.h - TikuKits text compression test interface
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

#ifndef TEST_KITS_TEXTCOMPRESSION_H_
#define TEST_KITS_TEXTCOMPRESSION_H_

#include <stdint.h>
#include <string.h>
#include "tests/tiku_test_harness.h"
#include "tikukits/textcompression/tiku_kits_textcompression.h"
#include "tikukits/textcompression/rle/tiku_kits_textcompression_rle.h"
#include "tikukits/textcompression/bpe/tiku_kits_textcompression_bpe.h"
#include "tikukits/textcompression/heatshrink/tiku_kits_textcompression_heatshrink.h"

/*---------------------------------------------------------------------------*/
/* TEST DECLARATIONS                                                         */
/*---------------------------------------------------------------------------*/

void test_kits_textcomp_rle(void);
void test_kits_textcomp_bpe(void);
void test_kits_textcomp_heatshrink(void);
void test_kits_textcomp_null(void);

#endif /* TEST_KITS_TEXTCOMPRESSION_H_ */
