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

#include "connector.h"

namespace SpaE
{

using Seconds = double;

Seconds now();
Seconds uptime();

class Timer;

struct TimerStatus
{
    bool        running = false;
    bool        singleShot = true;

    Seconds     timeout = 1;
    Seconds     lastEmitTime = 0;

    Timer       *timer = nullptr;

    SpinMutex   mutex;
};

using SharedTimerStatus = std::shared_ptr<TimerStatus>;

class Timer : public Object
{
public:
    Timer();
    ~Timer();

    void        start(const Seconds &sec);
    void        stop();

    bool        getRuning();

    Seconds     getTimeOut();
    Seconds     getRemaining();

    void        setSingleShot(bool sta);
    bool        getSingleShot();

signals:
    Signal<>        signalTimeout;

private:
    SharedTimerStatus       m_sharedTimerStatus;
};

// auto delete
Timer *setTimeout(const Seconds &sec, const std::function<void ()> &f);
Timer *setTimeout(const Seconds &sec, std::function<void ()> &&f);

Timer *setInterval(const Seconds &sec, const std::function<void ()> &f);
Timer *setInterval(const Seconds &sec, std::function<void ()> &&f);

void deleteTimer(Timer *t);

};
