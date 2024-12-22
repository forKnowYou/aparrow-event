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

#include <SpaE/coroutine_pool.h>

using namespace SpaE;

static int g_poolSize = std::thread::hardware_concurrency();
static int g_stackSize = 0;

static SpinMutex                                g_poolMapMutex;
static std::multimap<float, Coroutine *>        g_poolMap;
static std::unordered_map<Coroutine *, float>   g_coroutineWorkSizeMap;

void CoroutinePool::setPoolSize(int size)
{
    g_poolSize = size;

    std::unique_lock<decltype(g_poolMapMutex)>      lk(g_poolMapMutex);

    if ((int) g_poolMap.size() < size) {
        for (size_t i = 0; i < size - g_poolMap.size(); i ++) {
            char name[64];
            sprintf(name, "SpaE::Co::Pool%d", (int) i + (int) g_poolMap.size());

            auto co = Coroutine::newInstance(name);
            auto workSize = 0 + (i + g_poolMap.size()) * 0.000001;
            g_poolMap.emplace(workSize, co);
            g_coroutineWorkSizeMap.emplace(co, workSize);
        }
    }
}

int CoroutinePool::getPoolSize()
{
    return g_poolSize;
}

void CoroutinePool::setStackSize(int size)
{
    g_stackSize = size;
}

int CoroutinePool::getStackSize()
{
    return g_stackSize;
}

static void initPool()
{
    static bool init = false;

    if (! init) {
        init = true;

        std::unique_lock<decltype(g_poolMapMutex)>      lk(g_poolMapMutex);

        if ((int) g_poolMap.size() < g_poolSize) {
            for (size_t i = 0; i < g_poolSize - g_poolMap.size(); i ++) {
                char name[64];
                sprintf(name, "SpaE::Co::Pool%d", (int) i + (int) g_poolMap.size());

                auto co = Coroutine::newInstance(name);
                auto workSize = 0 + (i + g_poolMap.size()) * 0.000001;
                g_poolMap.emplace(workSize, co);
                g_coroutineWorkSizeMap.emplace(co, workSize);
            }
        }
    }
}

static inline void updateCoroutinePool(SharedContext &sc, Coroutine *co)
{
    auto workSizeIt = g_coroutineWorkSizeMap.find(co);

    g_poolMap.erase(workSizeIt->second);

    workSizeIt->second += 1;

    g_poolMap.emplace(workSizeIt->second, co);

    connect(sc.get(), &sc->signalCompelte,
        [=]
        {
            std::unique_lock<decltype(g_poolMapMutex)>      lk(g_poolMapMutex);

            auto workSizeIt = g_coroutineWorkSizeMap.find(co);
            auto it = g_poolMap.find(workSizeIt->second);

            g_poolMap.erase(it);

            workSizeIt->second -= 1;

            g_poolMap.emplace(workSizeIt->second, co);
        }
    );
}

CoroutinePool::ContextInfo CoroutinePool::coroutineWork(const Loop::WorkFun &f, const int &stackSize, const Loop::Priority &pri)
{
    initPool();

    ContextInfo     info;

    SharedContext       sc;
    {
        std::unique_lock<decltype(g_poolMapMutex)>      lk(g_poolMapMutex);

        auto it = g_poolMap.begin();
        auto co = it->second;
        sc = it->second->work(f, stackSize, pri);

        updateCoroutinePool(sc, co);

        info.coroutine = it->second;
        info.sharedContext = std::move(sc);
    }

    return info;
}

CoroutinePool::ContextInfo CoroutinePool::coroutineWork(Loop::WorkFun &&f, const int &stackSize, const Loop::Priority &pri)
{
    initPool();

    ContextInfo     info;

    SharedContext       sc;
    {
        std::unique_lock<decltype(g_poolMapMutex)>      lk(g_poolMapMutex);

        auto it = g_poolMap.begin();
        auto co = it->second;
        sc = it->second->work(std::move(f), stackSize, pri);

        updateCoroutinePool(sc, co);

        info.coroutine = it->second;
        info.sharedContext = std::move(sc);
    }

    return info;
}

static inline void updateLoopPool(Coroutine *co)
{
    auto workSizeIt = g_coroutineWorkSizeMap.find(co);

    g_poolMap.erase(workSizeIt->second);

    workSizeIt->second += 1;

    g_poolMap.emplace(workSizeIt->second, co);

    co->getLoop()->work(
        [=]
        {
            std::unique_lock<decltype(g_poolMapMutex)>      lk(g_poolMapMutex);

            auto workSizeIt = g_coroutineWorkSizeMap.find(co);
            auto it = g_poolMap.find(workSizeIt->second);

            g_poolMap.erase(it);

            workSizeIt->second -= 1;

            g_poolMap.emplace(workSizeIt->second, co);
        }
    );
}

void CoroutinePool::loopWork(const Loop::WorkFun &f, const Loop::Priority &pri)
{
    initPool();

    {
        std::unique_lock<decltype(g_poolMapMutex)>      lk(g_poolMapMutex);

        auto it = g_poolMap.begin();
        auto co = it->second;

        it->second->getLoop()->work(f, pri);

        updateLoopPool(co);
    }
}

void CoroutinePool::loopWork(Loop::WorkFun &&f, const Loop::Priority &pri)
{
    initPool();

    {
        std::unique_lock<decltype(g_poolMapMutex)>      lk(g_poolMapMutex);

        auto it = g_poolMap.begin();
        auto co = it->second;

        it->second->getLoop()->work(std::move(f), pri);

        updateLoopPool(co);
    }
}
