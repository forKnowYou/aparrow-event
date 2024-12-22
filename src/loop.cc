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

#include <SpaE/loop.h>

#include <unordered_map>

using namespace SpaE;

static std::unordered_map<std::thread::id, Loop *>      g_loopPoolMap;
static SpinMutex    g_loopPoolMutex;

Loop::Loop(const char *name)
{
    if(name) {
        m_name = name;
    }
    else {
        m_name = "anonymous";
    }

    m_sharedAlive = std::make_shared<LoopAlive>();
    m_sharedAlive->onDelete =
        [=]
        {
            m_deleteSem.post();
        };

    std::unique_lock<decltype(g_loopPoolMutex)> lk(g_loopPoolMutex);

    m_thread = std::thread(
        [=]
        {
            printf("run event loop %s, thread_id=%lu(%lx) \r\n", m_name.data(), (unsigned long int) pthread_self(), (unsigned long int) pthread_self());

            run();
        }
    );
    pthread_setname_np(m_thread.native_handle(), m_name.data());

    g_loopPoolMap.emplace(m_thread.get_id(), this);
}

Loop::~Loop()
{
    {
        std::unique_lock<decltype(g_loopPoolMutex)>     lk(g_loopPoolMutex);

        g_loopPoolMap.erase(m_thread.get_id());
    }
}

void Loop::work(const WorkFun &w, const Priority pri)
{
    std::unique_lock<decltype(m_operateMutex)> lock(m_operateMutex);

    workHelper(w, pri);
}

void Loop::work(WorkFun &&w, const Priority pri)
{
    std::unique_lock<decltype(m_operateMutex)> lock(m_operateMutex);

    workHelper(std::move(w), pri);
}

void Loop::workSync(const WorkFun &w, const Priority pri)
{
    if (getCurrentLoop() == this) {
        process();

        w();
        return;
    }

    m_operateMutex.lock();

    Semaphore sem;

    workHelper(w, pri);
    workHelper(
        [&]
        {
            sem.post();
        }
    , pri);

    m_operateMutex.unlock();

    sem.wait();
}

void Loop::workSync(WorkFun &&w, const Priority pri)
{
    if (getCurrentLoop() == this) {
        process();

        w();
        return;
    }

    m_operateMutex.lock();

    Semaphore sem;

    workHelper(std::move(w), pri);
    workHelper(
        [&]
        {
            sem.post();
        }
    , pri);

    m_operateMutex.unlock();

    sem.wait();
}

void Loop::addSharedConnectBase(const SharedConnectBase &sc)
{
    std::unique_lock<decltype(m_operateMutex)> lock(m_operateMutex);

    m_sharedConnectBaseSet.emplace(sc);
}

void Loop::removeSharedConnectBase(const SharedConnectBase &sc)
{
    std::unique_lock<decltype(m_operateMutex)> lock(m_operateMutex);

    m_sharedConnectBaseSet.erase(sc);
}

void Loop::setRun(bool sta)
{
    if (sta) {
        m_runStaSem.post();
    }
    else {
        work(
            [=]
            {
                m_runStaSem.wait();
            }
        );
    }
}

std::thread &Loop::getThread()
{
    return m_thread;
}

int Loop::queueSize()
{
    std::unique_lock<decltype(m_operateMutex)> lock(m_operateMutex);

    return m_eventsMap.size();
}

void Loop::waitEvent()
{
    if (this != getCurrentLoop()) {
        throw std::runtime_error("Loop::process() incorrect call in another thread \r\n");
    }

    m_runSem.wait();
}

void Loop::process()
{
    if (this != getCurrentLoop()) {
        throw std::runtime_error("Loop::process() incorrect call in another thread \r\n");
    }

    if (! m_runSem.tryWait()) {
        return;
    }

    processData();
}

void Loop::waitProcess()
{
    if (this != getCurrentLoop()) {
        throw std::runtime_error("Loop::waitProcess() incorrect call in another thread \r\n");
    }

    m_runSem.wait();

    processData();
}

const char *Loop::getName()
{
    return m_name.data();
}

SharedLoopAlive Loop::getSharedAlive()
{
    return m_sharedAlive;
}

Loop *Loop::getCurrentLoop()
{
    {
        std::unique_lock<decltype(g_loopPoolMutex)> lk(g_loopPoolMutex);

        auto it = g_loopPoolMap.find(std::this_thread::get_id());

        if (it != g_loopPoolMap.end()) {
            return it->second;
        }
    }

    return getInstance();
}

Loop *Loop::getInstance()
{
    static Loop *instance = nullptr;

    if (! instance) {
        instance = new Loop("SpaE:Def");
    }

    return instance;
}

Loop *Loop::newInstance(const char *name)
{
    return new Loop(name);
}

void Loop::deleteLater()
{
    {
        std::unique_lock<decltype(m_operateMutex)> lock(m_operateMutex);

        for (auto &it: m_sharedConnectBaseSet) {
            auto &sc = *it;

            sc.disconnectFun(it);
        }
        m_sharedConnectBaseSet.clear();
    }

    m_sharedAlive = nullptr;

    // 自动退出线程
    m_terminate = true;

    setRun(true);

    // 添加一个空事件让处理线程能够运行并正常退出
    work(nullptr);
}

void Loop::workHelper(const WorkFun &w, const Priority pri)
{
    if (pri == HighPriority) {
        m_highPriWorkTable.push(w);
    }
    else {
        m_eventsMap.emplace(pri, w);
    }

    // 让事件处理线程执行
    m_runSem.post();
}

void Loop::workHelper(WorkFun &&w, const Priority pri)
{
    if (pri == HighPriority) {
        m_highPriWorkTable.push(std::move(w));
    }
    else {
        m_eventsMap.emplace(pri, std::move(w));
    }

    // 让事件处理线程执行
    m_runSem.post();
}

void Loop::run()
{
    while (! m_terminate) {
        m_runSem.wait();

        processData();
    }

    m_deleteSem.wait();

    delete this;
}

void Loop::processData()
{
    do {
        // 从队列中弹出事件
        if (m_highPriWorkTable.pop(m_workFun)) {

        }
        else {
            std::unique_lock<decltype(m_operateMutex)>      lk(m_operateMutex);

            if (m_eventsMap.size()) {
                m_eventsMapIt = m_eventsMap.begin();
            }
            else {
                break;
            }

            m_workFun = std::move(m_eventsMapIt->second);
            m_eventsMap.erase(m_eventsMapIt);
        }

        // 执行事件
        if (m_workFun) {
            m_workFun();
        }
    } while(m_runSem.tryWait());
}
