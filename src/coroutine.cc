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

#include <SpaE/coroutine.h>

#include "context.h"

#define DBG     0

#if DBG
#define DBG_LOG(fmt, ...)        printf("%.6f " fmt, uptime(), __VA_ARGS__)
#else
#define DBG_LOG(...)
#endif

using namespace SpaE;

static std::atomic<uint64_t>    g_contextId { 1 };

static std::unordered_map<std::thread::id, Coroutine *>      g_coPoolMap;
static SpinMutex    g_coPoolMutex;

struct SpaE::ArchContext {
    std::vector<char>       stack;

    tb_context_ref_t        ref;
    tb_context_from_t       from;
};

#define STACK_OVERFLOW_MARK     (0x55aaaa55)

static std::unordered_set<Context *>    g_contextSet;
static SpinMutex    g_contextSetMutex;

static void archContextFun(tb_context_from_t from)
{
    auto ctx = (Context *) from.priv;
    auto sc = Coroutine::getCurrentCoroutine()->getCurrentContext();

    {
        std::unique_lock<decltype(g_contextSetMutex)>       lk(g_contextSetMutex);

        g_contextSet.emplace(ctx);
    }

    sc->archContex->from = from;

    // from may change in work
    ctx->work();

    {
        std::unique_lock<decltype(sc->mutex)>       lk(sc->mutex);

        sc->alive = false;
    }

    sc->compeleteCv.notify_all();

    emit sc->signalCompelte();

    {
        std::unique_lock<decltype(g_contextSetMutex)>       lk(g_contextSetMutex);

        g_contextSet.erase(ctx);
    }

    tb_context_jump(sc->archContex->from.context, nullptr);
}

bool Coroutine::stackOverflowCheck(const char **loopName, int *stackSize)
{
    for (auto &it: g_contextSet) {
        if (((uint32_t *) (& it->archContex->stack[it->archContex->stack.size() - 4]))[0] == STACK_OVERFLOW_MARK) {
            continue;
        }

        *loopName = it->getLoop()->getName();
        *stackSize = it->archContex->stack.size();

        return true;
    }
    return false;
}

Context::Context(const Loop::WorkFun &work, int stackSize)
{
    this->work = work;

    init(stackSize);
}

Context::Context(Loop::WorkFun &&work, int stackSize)
{
    this->work = std::move(work);

    init(stackSize);
}

Context::~Context()
{
    compeleteCv.notify_all();

    delete archContex;
}

void Context::init(int stackSize)
{
    id = g_contextId ++;
    pri = 0;
    alive = true;
    firstRun = true;
    running = true;

    auto archContex = new ArchContext();
    archContex->stack.resize(stackSize);
    ((uint32_t *) (& archContex->stack[stackSize - 4]))[0] = STACK_OVERFLOW_MARK;
    archContex->ref = tb_context_make(archContex->stack.data(), stackSize, archContextFun);

    this->archContex = archContex;
}

// ################################################################

Coroutine::Coroutine(const char *name)
{
    m_loop = Loop::newInstance(name);

    m_loop->work(std::bind(&Coroutine::run, this));

    std::unique_lock<decltype(g_coPoolMutex)>       lk(g_coPoolMutex);

    g_coPoolMap.emplace(m_loop->getThread().get_id(), this);
}

Coroutine::~Coroutine()
{
    m_loop->deleteLater();

    std::unique_lock<decltype(g_coPoolMutex)>       lk(g_coPoolMutex);

    g_coPoolMap.erase(m_loop->getThread().get_id());
}

SharedContext Coroutine::work(const Loop::WorkFun &f, const int &stackSize, const Loop::Priority &pri)
{
    auto ss = stackSize;
    if (ss < m_stackSize) {
        ss = m_stackSize;
    }

    auto sc = std::make_shared<Context>(std::move(f), ss);
    sc->moveToLoop(m_loop);

    m_loop->work(
        [=]
        {
            m_sharedContextSet.emplace(sc);

            m_runningContextMap.emplace(sc->pri, sc);
        }
    );

    return sc;
}

SharedContext Coroutine::work(Loop::WorkFun &&f, const int &stackSize, const Loop::Priority &pri)
{
    auto ss = stackSize;
    if (ss < m_stackSize) {
        ss = m_stackSize;
    }

    auto sc = std::make_shared<Context>(std::move(f), ss);
    sc->moveToLoop(m_loop);

    m_loop->work(
        [=]
        {
            m_sharedContextSet.emplace(sc);

            m_runningContextMap.emplace(sc->pri, sc);
        }
    );

    return sc;
}

void Coroutine::join(const SharedContext &sc)
{
    auto co = getCurrentCoroutine();

    if (! co) {
        std::unique_lock<decltype(sc->compeleteCvMutex)>        lk(sc->compeleteCvMutex);
        sc->compeleteCv.wait(lk,
            [=]
            {
                return !sc->alive;
            }
        );
        return;
    }

    {
        auto coSc = co->getCurrentContext();

        std::unique_lock<decltype(sc->mutex)>       lk(sc->mutex);

        if (! sc->alive) {
            return;
        }

        connect(sc.get(), &sc->signalCompelte,
            [=]
            {
                co->resume(coSc);
            }
        );
    }

    pending();
}

void Coroutine::resume(const SharedContext &sc)
{
    DBG_LOG("%s %d: %lld \r\n", __FUNCTION__, __LINE__, (long long) sc->id);

    m_loop->work(
        [=]
        {
            auto it = m_sharedContextSet.find(sc);
            if (it == m_sharedContextSet.end()) {
                return;
            }

            if (! sc->alive || sc->running) {
                return;
            }

            DBG_LOG("%s %d: %lld \r\n", __FUNCTION__, __LINE__, (long long) sc->id);

            sc->running = true;
            m_runningContextMap.emplace(sc->pri, sc);
        }
    );

    if (this == getCurrentCoroutine()) {
        if (m_currentContext) {
            yield();
        }
    }
}

void Coroutine::pending()
{
    auto co = getCurrentCoroutine();
    if (! co) {
        printf("warning: SpaE::Coroutine::%s faild cause NOT_IN_COROUTINE \r\n", __FUNCTION__);

        return;
    }

    auto sc = co->getCurrentContext();

    DBG_LOG("%s %d: %lld \r\n", __FUNCTION__, __LINE__, (long long) sc->id);

    sc->archContex->from = tb_context_jump(sc->archContex->from.context, nullptr);
}

void Coroutine::yield()
{
    auto co = getCurrentCoroutine();
    if (! co) {
        std::this_thread::yield();

        return;
    }

    auto sc = co->getCurrentContext();

    DBG_LOG("%s %d: %lld \r\n", __FUNCTION__, __LINE__, (long long) sc->id);

    co->getLoop()->work(
        [=]
        {
            co->m_runningContextMap.emplace(sc->pri, sc);
        }
    );

    sc->archContex->from = tb_context_jump(sc->archContex->from.context, nullptr);
}

void Coroutine::yieldFor(const Seconds &sec)
{
    auto co = getCurrentCoroutine();
    if (! co) {
        printf("warning: SpaE::Coroutine::%s faild cause NOT_IN_COROUTINE \r\n", __FUNCTION__);

        return;
    }

    auto sc = co->getCurrentContext();

    setTimeout(sec,
        [=]
        {
            co->resume(sc);
        }
    );

    DBG_LOG("%s %d: %lld \r\n", __FUNCTION__, __LINE__, (long long) sc->id);

    pending();
}

Loop *Coroutine::getLoop()
{
    return m_loop;
}

SharedContext Coroutine::getCurrentContext()
{
    return m_currentContext;
}

Coroutine *Coroutine::getCurrentCoroutine()
{
    std::unique_lock<decltype(g_coPoolMutex)>       lk(g_coPoolMutex);

    auto it = g_coPoolMap.find(std::this_thread::get_id());
    if (it == g_coPoolMap.end()) {
        return nullptr;
    }
    return it->second;
}

Coroutine *Coroutine::newInstance(const char *name)
{
    return new Coroutine(name);
}

Coroutine *Coroutine::getInstance()
{
    static Coroutine *ins = nullptr;

    if (! ins) {
        ins = new Coroutine("SpaE:Co:Def");
    }

    return ins;
}

void Coroutine::deleteLater()
{
    m_loop->deleteLater();

    // 自动退出线程
    m_terminate = true;

    m_loop->setRun(true);

    // 添加一个空事件让处理线程能够运行并正常退出
    m_loop->work(nullptr);
}

void Coroutine::setStackSize(int s)
{
    m_stackSize = s;
}

int Coroutine::getStackSize()
{
    return m_stackSize;
}

void Coroutine::setRun(bool sta)
{
    m_loop->setRun(sta);
}

int Coroutine::workSetSize()
{
    return m_sharedContextSet.size();
}

void Coroutine::run()
{
    while (! m_terminate) {
        auto it = m_runningContextMap.begin();
        if (it == m_runningContextMap.end()) {
            m_loop->waitProcess();

            // check if there new context get
            continue;
        }
        else {
            m_loop->process();
        }

        m_currentContext = std::move(it->second);

        m_runningContextMap.erase(it);

        DBG_LOG("%s %d: %lld \r\n", __FUNCTION__, __LINE__, (long long) m_currentContext->id);

        if (m_currentContext->firstRun) {
            m_currentContext->firstRun = false;

            m_currentContext->archContex->from = tb_context_jump(m_currentContext->archContex->ref, m_currentContext.get());
        }
        else {
            m_currentContext->archContex->from = tb_context_jump(m_currentContext->archContex->from.context, nullptr);
        }

        m_currentContext->running = false;

        if (! m_currentContext->alive) {
            m_sharedContextSet.erase(m_currentContext);
        }

        m_currentContext = nullptr;
    }

    delete this;
}
