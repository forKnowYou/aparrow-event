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

#include <SpaE/fd_operator.h>

using namespace SpaE;

#ifdef __linux__

#include <sys/epoll.h>
#include <unistd.h>
#include <termios.h>

#include <thread>

#define EpollSize       8

struct FdOperatorAliveInfo {
    SharedAliveMutex    alive;

    FdOperator      *o;
};

static std::unordered_map<ObjectId, FdOperatorAliveInfo>    g_fdOperatorMap;
static SpinMutex        g_fdOperatorMapMutex;

static int getEpollFd()
{
    static int fd = 0;
    if(! fd) {
        fd = epoll_create(EpollSize);

        auto el = Loop::newInstance("SpaE::FdE");

        el->work(
            [=]
            {
                struct epoll_event events[EpollSize];

                while(true) {
                    auto ret = epoll_wait(fd, events, EpollSize, -1);

                    for(auto i = 0; i < ret; i ++) {
                        auto id = events[i].data.u64;
                        {
                            std::unique_lock<decltype(g_fdOperatorMapMutex)>    lk(g_fdOperatorMapMutex);

                            auto it = g_fdOperatorMap.find(id);
                            if (it == g_fdOperatorMap.end()) {
                                continue;
                            }

                            auto &info = it->second;
                            {
                                std::unique_lock<decltype(info.alive->mutex)>       lk(info.alive->mutex);

                                if (info.alive->alive) {
                                    emit info.o->signalEpollWatch(events[i].events);
                                }
                            }
                        }
                    }
                }
            }
        );
    }
    return fd;
}

static int getInotifyEpollFd()
{
    static int fd = 0;
    static char buf[1024];

    if(! fd) {
        fd = epoll_create(EpollSize);

        auto el = Loop::newInstance("SpaE::FdI");

        el->work([=] {
            struct epoll_event events[EpollSize];

            while(true) {
                auto ret = epoll_wait(fd, events, EpollSize, -1);

                for(auto i = 0; i < ret; i ++) {
                    auto id = events[i].data.u64;
                    {
                        std::unique_lock<decltype(g_fdOperatorMapMutex)>    lk(g_fdOperatorMapMutex);

                        auto it = g_fdOperatorMap.find(id);
                        if (it == g_fdOperatorMap.end()) {
                            continue;
                        }

                        auto &info = it->second;
                        {
                            std::unique_lock<decltype(info.alive->mutex)>       lk(info.alive->mutex);

                            if (info.alive->alive) {
                                auto len = ::read(info.o->getInotifyFd(), buf, sizeof(buf));
                                char *p = buf;

                                for(; p < (buf + len); ) {
                                    auto event = (struct inotify_event *) p;

                                    emit info.o->signalInotifyWatch(event->mask);

                                    p += sizeof(struct inotify_event) + event->len;
                                }
                            }
                        }
                    }
                }
            }
        });
    }
    return fd;
}

FdOperator::FdOperator(int fd, const char *path)
{
    m_fd = fd;
    m_path = path;
}

FdOperator::FdOperator(const char *path, int o_flags)
{
    m_path = path;

    m_fd = ::open(path, o_flags);
    if(m_fd < 0) {
        char buf[256];
        sprintf(buf, "FdOperator open file(%s) error: ", path);
        perror(buf);
    }
}

FdOperator::~FdOperator()
{
    ::close(m_watchEpollFd);
    ::close(m_watchInotifyFd);
    ::close(m_watchInotifyEpollFd);
    ::close(m_fd);
}

int FdOperator::write(const void *buf, size_t len)
{
    return ::write(m_fd, buf, len);
}

int FdOperator::read(void *buf, size_t len)
{
    return ::read(m_fd, buf, len);
}

void FdOperator::configSerial()
{
    struct termios attr;
    tcgetattr(m_fd, &attr);

    attr.c_cflag |= CLOCAL | CREAD;
    attr.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    attr.c_oflag &= ~OPOST;
    attr.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

    tcsetattr(m_fd, TCSANOW, &attr);
}

int FdOperator::getFd()
{
    return m_fd;
}

int FdOperator::getInotifyFd()
{
    return m_watchInotifyFd;
}

void FdOperator::epollWatch(int flags, bool isolate)
{
    struct epoll_event ev = {0};
    ev.events = flags;
    ev.data.u64 = getId();

    if(! isolate) {
        auto fd = getEpollFd();

        FdOperatorAliveInfo info;
        info.alive = getSharedAliveMutex();
        info.o = this;

        {
            std::unique_lock<decltype(g_fdOperatorMapMutex)>    lk(g_fdOperatorMapMutex);

            g_fdOperatorMap.emplace((ObjectId) ev.data.u64, std::move(info));
        }

        epoll_ctl(fd, EPOLL_CTL_ADD, m_fd, &ev);
    }
    else {
        (new std::thread([=]
            {
                char nameBuf[64];
                sprintf(nameBuf, "SpaE::FdW::%d", m_fd);

                pthread_setname_np(pthread_self(), nameBuf);

                epoll_event events[1];
                auto tempEv = ev;

                m_watchEpollFd = epoll_create(1);

                epoll_ctl(m_watchEpollFd, EPOLL_CTL_ADD, m_fd, &tempEv);

                while(true) {
                    auto ret = epoll_wait(m_watchEpollFd, events, 1, -1);

                    if(ret < 0) {
                        if(errno == EBADF) {
                            break;
                        }
                    }

                    for(auto i = 0; i < ret; i ++) {
                        auto id = events[i].data.u64;
                        {
                            std::unique_lock<decltype(g_fdOperatorMapMutex)>    lk(g_fdOperatorMapMutex);

                            auto it = g_fdOperatorMap.find(id);
                            if (it == g_fdOperatorMap.end()) {
                                continue;
                            }

                            auto &info = it->second;
                            {
                                std::unique_lock<decltype(info.alive->mutex)>       lk(info.alive->mutex);

                                if (info.alive->alive) {
                                    emit info.o->signalEpollWatch(events[i].events);
                                }
                            }
                        }
                    }
                }
            }
        ))->detach();
    }
}

void FdOperator::inotifyWatch(int flags, bool isolate)
{
    struct epoll_event ev = {0};
    ev.events = EPOLLIN;
    ev.data.u64 = getId();

    m_watchInotifyFd = inotify_init();
    auto path = m_path.data();
    inotify_add_watch(m_watchInotifyFd, path, flags);

    if(! isolate) {
        auto fd = getInotifyEpollFd();

        FdOperatorAliveInfo info;
        info.alive = getSharedAliveMutex();
        info.o = this;

        {
            std::unique_lock<decltype(g_fdOperatorMapMutex)>    lk(g_fdOperatorMapMutex);

            g_fdOperatorMap.emplace((ObjectId) ev.data.u64, std::move(info));
        }

        epoll_ctl(fd, EPOLL_CTL_ADD, m_watchInotifyFd, &ev);
    }
    else {
        (new std::thread([=]
            {
                char nameBuf[64];
                sprintf(nameBuf, "SpaE::FdW::%d", m_fd);

                pthread_setname_np(pthread_self(), nameBuf);

                epoll_event events[1];
                auto tempEv = ev;
                char buf[1024];

                m_watchInotifyEpollFd = epoll_create(1);

                epoll_ctl(m_watchInotifyEpollFd, EPOLL_CTL_ADD, m_watchInotifyFd, &tempEv);

                while(true) {
                    auto ret = epoll_wait(m_watchInotifyEpollFd, events, 1, -1);

                    if(ret < 0) {
                        if(errno == EBADF) {
                            break;
                        }
                    }

                    for(auto i = 0; i < ret; i ++) {
                        auto id = events[i].data.u64;
                        {
                            std::unique_lock<decltype(g_fdOperatorMapMutex)>    lk(g_fdOperatorMapMutex);

                            auto it = g_fdOperatorMap.find(id);
                            if (it == g_fdOperatorMap.end()) {
                                continue;
                            }

                            auto &info = it->second;
                            {
                                std::unique_lock<decltype(info.alive->mutex)>       lk(info.alive->mutex);

                                if (info.alive->alive) {
                                    auto len = ::read(info.o->getInotifyFd(), buf, sizeof(buf));
                                    char *p = buf;

                                    for(; p < (buf + len); ) {
                                        auto event = (struct inotify_event *) p;

                                        emit info.o->signalInotifyWatch(event->mask);

                                        p += sizeof(struct inotify_event) + event->len;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        ))->detach();
    }
}

#endif
