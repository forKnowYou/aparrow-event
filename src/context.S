/*!The Sparrow Event Library
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Copyright (C) 2024-present, bluewings.
 *
 */

#if defined(_M_IX86) || defined(__i386__)
#   include "x86/context.S"
#elif defined(_M_X64) || defined(__x86_64__)
#   include "x64/context.S"
#elif defined(__aarch64__)
#   include "arm64/context.S"
#elif defined(__arm__)
#   include "arm/context.S"
#endif
