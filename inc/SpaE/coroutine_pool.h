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

#pragma once

#include "coroutine.h"

namespace SpaE
{

class CoroutinePool
{
public:
    struct ContextInfo {
        Coroutine       *coroutine;

        SharedContext   sharedContext;
    };

    static void         setPoolSize(int size);
    static int          getPoolSize();
    static void         setStackSize(int size);
    static int          getStackSize();

    static ContextInfo      coroutineWork(const Loop::WorkFun &f, const int &stackSize = 0, const Loop::Priority &pri = 0);
    static ContextInfo      coroutineWork(Loop::WorkFun &&f, const int &stackSize = 0, const Loop::Priority &pri = 0);
    static void             loopWork(const Loop::WorkFun &f, const Loop::Priority &pri = 0);
    static void             loopWork(Loop::WorkFun &&f, const Loop::Priority &pri = 0);
};

} // namespace SpaE
