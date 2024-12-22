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

#include <vector>
#include <condition_variable>

#include "loop.h"
#include "timer.h"

namespace SpaE
{

using ContextId = uint64_t;

struct ArchContext;

class Coroutine;

struct Context : public Object
{
    std::vector<char>       stack;

    ContextId       id;

    Loop::WorkFun   work;
    Loop::Priority  pri;

    bool        alive;
    bool        firstRun;
    bool        running;

    std::mutex                  compeleteCvMutex;
    std::condition_variable     compeleteCv;

    SpinMutex       mutex;

    ArchContext     *archContex;

    Context(const Loop::WorkFun &work, int stackSize);
    Context(Loop::WorkFun &&work, int stackSize);
    ~Context();

    Context(const Context &) = delete;
    Context& operator= (const Context&) = delete;

    void init(int stackSize);

    bool operator < (const Context &other) const
    {
        return id < other.id;
    }

    bool operator == (const Context &other) const
    {
        return id == other.id;
    }

    void        join();

    Signal<>    signalCompelte;
};

using SharedContext = std::shared_ptr<Context>;

class Coroutine
{
public:
    Coroutine(const Coroutine &) = delete;
    Coroutine& operator= (const Coroutine&) = delete;

    SharedContext       work(const Loop::WorkFun &f, const int &stackSize = 0, const Loop::Priority &pri = 0);
    SharedContext       work(Loop::WorkFun &&f, const int &stackSize = 0, const Loop::Priority &pri = 0);

    void    join(const SharedContext &sc);
    void    resume(const SharedContext &sc);

    static void     pending();
    static void     yield();
    static void     yieldFor(const Seconds &sec);

    Loop        *getLoop();

    SharedContext       getCurrentContext();

    static Coroutine    *getCurrentCoroutine();

    static Coroutine    *newInstance(const char *name = "SpaE::Co");
    static Coroutine    *getInstance();

    void    deleteLater();

    void    setStackSize(int s);
    int     getStackSize();

    void    setRun(bool sta);

    int     workSetSize();

    static bool     stackOverflowCheck(const char **loopName, int *stackSize);

private:
    Coroutine(const char *name = "SpaE::Co");
    ~Coroutine();

    void    run();

private:
    Loop    *m_loop;

    int     m_stackSize = 64 * 1024;

    std::multimap<Loop::Priority, SharedContext>        m_runningContextMap;

    std::unordered_set<SharedContext>       m_sharedContextSet;

    bool        m_terminate = false;

    SharedContext       m_currentContext;

    SpinMutex       m_mutex;

    Semaphore       m_runStaSem;
};

};

namespace std
{
    template<>
    struct hash<SpaE::Context>
    {
        size_t operator () (const SpaE::Context &o) const
        {
            return o.id;
        }
    };
};
