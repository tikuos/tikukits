/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * test_kits_sigfeatures.h - TikuKits signal features test interface
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

#ifndef TEST_KITS_SIGFEATURES_H_
#define TEST_KITS_SIGFEATURES_H_

#include <stdint.h>
#include <string.h>
#include "tests/tiku_test_harness.h"
#include "tikukits/sigfeatures/tiku_kits_sigfeatures.h"
#include "tikukits/sigfeatures/peak/tiku_kits_sigfeatures_peak.h"
#include "tikukits/sigfeatures/zcr/tiku_kits_sigfeatures_zcr.h"
#include "tikukits/sigfeatures/histogram/tiku_kits_sigfeatures_histogram.h"
#include "tikukits/sigfeatures/delta/tiku_kits_sigfeatures_delta.h"
#include "tikukits/sigfeatures/goertzel/tiku_kits_sigfeatures_goertzel.h"
#include "tikukits/sigfeatures/zscore/tiku_kits_sigfeatures_zscore.h"
#include "tikukits/sigfeatures/scale/tiku_kits_sigfeatures_scale.h"

/*---------------------------------------------------------------------------*/
/* TEST DECLARATIONS                                                         */
/*---------------------------------------------------------------------------*/

void test_kits_sigfeatures_peak(void);
void test_kits_sigfeatures_zcr(void);
void test_kits_sigfeatures_histogram(void);
void test_kits_sigfeatures_delta(void);
void test_kits_sigfeatures_goertzel(void);
void test_kits_sigfeatures_zscore(void);
void test_kits_sigfeatures_scale(void);
void test_kits_sigfeatures_null(void);

#endif /* TEST_KITS_SIGFEATURES_H_ */
