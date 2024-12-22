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

#include <SpaE/timer.h>

#include <vector>

using namespace SpaE;

static Semaphore        g_helperSem;

static std::mutex       g_timerMapMutex;

static std::multimap<Seconds, SharedTimerStatus>     g_timerMap;

static auto g_uptime = now();

static void helperThread()
{
    Seconds     waitTime = 666666;

    std::vector<SharedTimerStatus>      readyTimerArray;

    decltype(g_timerMap)::iterator      it;

    for (;;) {
        g_helperSem.waitFor(waitTime);

        {
            std::unique_lock<decltype(g_timerMapMutex)>     lk(g_timerMapMutex);

            auto n = now();

            it = g_timerMap.begin();
            while (it != g_timerMap.end()) {
                if (it->first > n) {
                    break;
                }

                readyTimerArray.emplace_back(it->second);

                it = g_timerMap.erase(it);
            }
        }

        for (auto &sts: readyTimerArray) {
            std::unique_lock<decltype(sts->mutex)>      lk(sts->mutex);

            if (sts->running) {
                emit sts->timer->signalTimeout();

                if (! sts->singleShot) {
                    auto n = now();
                    sts->lastEmitTime = n;

                    g_timerMap.emplace(sts->lastEmitTime + sts->timeout, sts);
                }
            }
        }
        readyTimerArray.clear();

        {
            std::unique_lock<decltype(g_timerMapMutex)>     lk(g_timerMapMutex);

            it = g_timerMap.begin();
            if (it != g_timerMap.end()) {
                waitTime = it->first - now();
                if (waitTime < 0) {
                    waitTime = 0;
                }
            }
            else {
                waitTime = 666666;
            }
        }
    }
}

static Loop *getHelperThread()
{
    static Loop *ins = nullptr;

    if (! ins) {
        ins = Loop::newInstance("SpaE::timer");

        ins->work(helperThread);
    }
    return ins;
}

Seconds SpaE::now()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count() / 1000000.0;
}

Seconds SpaE::uptime()
{
    return now() - g_uptime;
}

Timer::Timer()
{
    getHelperThread();

    m_sharedTimerStatus = std::make_shared<TimerStatus>();
    m_sharedTimerStatus->timer = this;
}

Timer::~Timer()
{
    std::unique_lock<decltype(m_sharedTimerStatus->mutex)>      lk(m_sharedTimerStatus->mutex);

    m_sharedTimerStatus->running = false;
}

void Timer::start(const Seconds &sec)
{
    stop();

    {
        std::unique_lock<decltype(m_sharedTimerStatus->mutex)>      lk(m_sharedTimerStatus->mutex);

        m_sharedTimerStatus->lastEmitTime = now();
        m_sharedTimerStatus->running = true;
        m_sharedTimerStatus->timeout = sec;
    }

    {
        std::unique_lock<decltype(g_timerMapMutex)>     lk(g_timerMapMutex);

        g_timerMap.emplace(m_sharedTimerStatus->lastEmitTime + m_sharedTimerStatus->timeout, m_sharedTimerStatus);
    }

    g_helperSem.post();
}

void Timer::stop()
{
    {
        std::unique_lock<decltype(m_sharedTimerStatus->mutex)>      lk(m_sharedTimerStatus->mutex);

        m_sharedTimerStatus->running = false;
    }

    {
        std::unique_lock<decltype(g_timerMapMutex)>     lk(g_timerMapMutex);

        auto t = m_sharedTimerStatus->lastEmitTime + m_sharedTimerStatus->timeout;
        auto it = g_timerMap.find(t);
        while (it != g_timerMap.end()) {
            if (t != it->first) {
                break;
            }

            auto &sts = it->second;
            if (sts->timer != this) {
                it ++;
                continue;
            }

            g_timerMap.erase(it);
            break;
        }
    }
}

bool Timer::getRuning()
{
    return m_sharedTimerStatus->running;
}

Seconds Timer::getTimeOut()
{
    return m_sharedTimerStatus->timeout;
}

Seconds Timer::getRemaining()
{
    std::unique_lock<decltype(m_sharedTimerStatus->mutex)>      lk(m_sharedTimerStatus->mutex);

    auto n = now();
    if (m_sharedTimerStatus->lastEmitTime + m_sharedTimerStatus->timeout < n) {
        return 0;
    }
    else {
        return m_sharedTimerStatus->lastEmitTime + m_sharedTimerStatus->timeout - n;
    }
}

void Timer::setSingleShot(bool sta)
{
    std::unique_lock<decltype(m_sharedTimerStatus->mutex)>      lk(m_sharedTimerStatus->mutex);

    m_sharedTimerStatus->singleShot = sta;
}

bool Timer::getSingleShot()
{
    return m_sharedTimerStatus->singleShot;
}

Timer *SpaE::setTimeout(const Seconds &sec, const std::function<void ()> &f)
{
    auto t = new Timer();

    connect(t, &t->signalTimeout, f);
    connect(t, &t->signalTimeout,
        [=]
        {
            delete t;
        }
    );

    t->start(sec);

    return t;
}

Timer *SpaE::setTimeout(const Seconds &sec, std::function<void ()> &&f)
{
    auto t = new Timer();

    connect(t, &t->signalTimeout, std::move(f));
    connect(t, &t->signalTimeout,
        [=]
        {
            delete t;
        }
    );

    t->start(sec);

    return t;
}

Timer *SpaE::setInterval(const Seconds &sec, const std::function<void ()> &f)
{
    auto t = new Timer();

    connect(t, &t->signalTimeout, f);

    t->setSingleShot(false);
    t->start(sec);

    return t;
}

Timer *SpaE::setInterval(const Seconds &sec, std::function<void ()> &&f)
{
    auto t = new Timer();

    connect(t, &t->signalTimeout, std::move(f));

    t->setSingleShot(false);
    t->start(sec);

    return t;
}

void SpaE::deleteTimer(Timer *t)
{
    delete t;
}
