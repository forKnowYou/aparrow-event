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

#include <functional>
#include <thread>
#include <mutex>
#include <string>
#include <map>
#include <set>
#include <memory>

#include "ring_list.h"

#include "spin_mutex.h"
#include "semaphore.h"

namespace SpaE
{

struct AliveMutex {
    SpinMutex   mutex;

    bool        alive = true;
};

using SharedAliveMutex = std::shared_ptr<AliveMutex>;

struct LoopAlive {
    std::function<void ()>      onDelete;

    ~LoopAlive()
    {
        if (onDelete) {
            onDelete();
        }
    }
};

using SharedLoopAlive = std::shared_ptr<LoopAlive>;

using ConnectId = uint64_t;

struct ConnectBase;

using SharedConnectBase = std::shared_ptr<ConnectBase>;
using SharedConnectBaseSet = std::set<SharedConnectBase>;

class Loop;

struct ConnectBase
{
    ConnectId   id;

    bool    alive;

    Loop                *receiverLoop;
    SharedLoopAlive     receiverLoopAlive;

    // don't capture self !!!
    std::function<void (SharedConnectBase sc)>      disconnectFun;

    bool operator < (const ConnectBase &other) const
    {
        return id < other.id;
    }

    bool operator == (const ConnectBase &other) const
    {
        return id == other.id;
    }
};

class Loop
{
public:
    using WorkFun = std::function<void ()>;
    using Priority = unsigned int;

    enum {
        HighPriority
    };

    Loop(const Loop &) = delete;
    Loop& operator= (const Loop&) = delete;

    /**
     * @brief               向工作队列添加事件
     * @param w             要添加的事件, function<void ()>
     * @param pri           事件优先级
     */
    void work(const WorkFun &w, const Priority pri = 0);

    void work(WorkFun &&w, const Priority pri = 0);

    /**
     * @brief               向工作队列添加事件并等待事件同步。如果在自身循环事件中调用，则会立即执行。
     * @param w             要添加的事件, function<void ()>
     * @param pri           事件优先级
     */
    void workSync(const WorkFun &w, const Priority pri = 0);

    void workSync(WorkFun &&w, const Priority pri = 0);

    /**
     * @brief               添加对象函数事件
     * @param o             对应的对象
     * @param f             对象的公共函数，比如 &C::F
     * @param pri           事件优先级
     * @param args          参数列表，一般是 std::placeholders::_1 系列参数，或是固定参数
     */
    template<typename Object, typename Callable, typename ... Args>
    void workClassFun(Object *o, Callable &&f, const Priority pri, Args ... args)
    {
        work(std::bind(std::forward<Callable>(f), o, std::forward<Args>(args) ...), pri);
    }

    /**
     * @brief               添加对象函数事件
     * @param o             对应的对象
     * @param f             对象的公共函数，比如 &C::F
     * @param pri           事件优先级
     */
    template<typename Object, typename Callable>
    void workClassFun(Object *o, Callable &&f, const Priority pri)
    {
        work(std::bind(std::forward<Callable>(f), o), pri);
    }

    void addSharedConnectBase(const SharedConnectBase &sc);

    // not thread safe
    void removeSharedConnectBase(const SharedConnectBase &sc);

    /**
     * @brief               事件循环执行控制，注意：使用信号量实现
     * @param sta           true 执行, false 暂停
     */
    void setRun(bool sta);

    /**
     * @brief               获取事件执行线程
     * @return              线程
     */
    std::thread &getThread();

    /**
     * @brief               获取事件队列大小
     * @return              事件队列大小
     */
    int queueSize();

    /**
     * @brief               手动等待事件
     *                      警告！！！将会阻塞线程
     */
    void waitEvent();

    /**
     * @brief               手动执行事件
     */
    void process();

    /**
     * @brief               手动等待并执行事件
     *                      警告！！！将会阻塞线程
     */
    void waitProcess();

    /**
     * @brief               获取线程的名字
     */
    const char *getName();

    SharedLoopAlive getSharedAlive();

    /**
     * @brief               获取当前线程的事件循环对象
     * @return              事件循环对象
     **/
    static Loop *getCurrentLoop();

    static Loop *getInstance();

    static Loop *newInstance(const char *name = nullptr);

    void deleteLater();

private:
    // 其他线程操作锁可能会引发未定义行为，故不可直接创建对象
    Loop(const char *name = nullptr);

    // 禁止直接delete，必须使用deleteLater
    ~Loop();

    void workHelper(const WorkFun &w, const Priority pri);
    void workHelper(WorkFun &&w, const Priority pri);

    void run();

    void processData();

private:
    SpinMutex   m_operateMutex;

    Semaphore   m_runSem,
                m_runStaSem,
                m_deleteSem;

    SharedLoopAlive     m_sharedAlive;

    SharedConnectBaseSet    m_sharedConnectBaseSet;

    ring_list<WorkFun>      m_highPriWorkTable;

    std::multimap<Priority, WorkFun>        m_eventsMap;

    std::multimap<Priority, WorkFun>::iterator      m_eventsMapIt;

    bool        m_terminate = false;

    std::thread m_thread;

    std::string m_name;
};

};

namespace std
{
    template<>
    struct hash<SpaE::ConnectBase>
    {
        size_t operator () (const SpaE::ConnectBase &conn) const
        {
            return conn.id;
        }
    };
};
