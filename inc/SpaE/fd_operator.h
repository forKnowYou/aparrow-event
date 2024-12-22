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

#ifdef __linux__

#include <functional>

#include <string.h>

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/inotify.h>

#include "connector.h"

namespace SpaE
{

class FdOperator : public Object
{
public:

public:
    FdOperator(int fd, const char *path);
    FdOperator(const char *path, int o_flgs = O_RDONLY);

    ~FdOperator();

    int write(const void *buf, size_t len);
    int read(void *buf, size_t len);

    void configSerial();

    int getFd();
    int getInotifyFd();

    void epollWatch(int flags, bool isolate = false);
    void inotifyWatch(int flags, bool isolate = false);

signals:
    // WATCH的事件
    Signal<int>     signalEpollWatch;
    Signal<int>     signalInotifyWatch;

protected:
    int m_fd = -1;

    std::string m_path = "";

    int m_watchEpollFd = -1;
    int m_watchInotifyFd = -1;
    int m_watchInotifyEpollFd = -1;
};

};

#else

#error other platform not support yet

#endif
