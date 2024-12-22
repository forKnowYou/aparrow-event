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

#include <type_traits>
#include <map>
#include <set>
#include <vector>
#include <tuple>

#include "object.h"

namespace SpaE
{

// 主要的特性检测结构体
template<typename T, typename = void>
struct is_std_function : std::false_type {};

// 特化版本，仅当T是std::function时才会实例化
template<typename R, typename... Args>
struct is_std_function<std::function<R(Args...)>, std::void_t<decltype(&std::function<R(Args...)>::operator())>>
    : std::true_type {};

template<typename ... Args>
class Signal : public SignalBase
{
public:
    using Func = std::function<void (Args ...)>;

    using ConnectFunMap = std::map<SharedConnectBase, Func>;
    using ConnectSet = std::set<SharedConnectBase>;

    Signal()
    {

    }

    Signal(const Signal &other)
    {

    }

    Signal &operator = (const Signal &other)
    {

    }

    void connect(const SharedConnectBase &conn, const Func &f)
    {
        std::unique_lock<decltype(m_mutex)>     lk(m_mutex);

        m_connectFunMap.emplace(conn, f);
        m_connectSet.emplace(conn);
    }

    void connect(const SharedConnectBase &conn, Func &&f)
    {
        std::unique_lock<decltype(m_mutex)>     lk(m_mutex);

        m_connectFunMap.emplace(conn, std::forward<Func>(f));
        m_connectSet.emplace(conn);
    }

    void removeConnect(const SharedConnectBase &conn) override
    {
        std::unique_lock<decltype(m_mutex)>     lk(m_mutex);

        auto it = m_connectFunMap.find(conn);
        if (it == m_connectFunMap.end()) {
            return;
        }

        m_connectSet.erase(it->first);
        m_connectFunMap.erase(it);
    }

    void dispatch(const Args &... args)
    {
        if (! m_loop) {
            return;
        }

        if (m_loop == Loop::getCurrentLoop()) {
            dispatchHelper(args ...);
        }
        else {
            auto alive = m_containerAlive;

            m_loop->work(
                [=]
                {
                    if (! alive->alive) {
                        return;
                    }

                    dispatchHelper(args ...);
                }
            );
        }
    }

    void dispatchSync(const Args & ... args)
    {
        if (! m_loop) {
            return;
        }

        if (m_loop == Loop::getCurrentLoop()) {
            dispatchSyncHelper(args ...);
        }
        else {
            auto alive = m_containerAlive;

            m_loop->workSync(
                [=]
                {
                    if (! alive->alive) {
                        return;
                    }

                    dispatchSyncHelper(args ...);
                }
            );
        }
    }

    void operator () (const Args & ... args)
    {
        dispatch(args ...);
    }

private:
    void dispatchHelper(const Args &... args)
    {
        auto sharedAlive = m_containerAlive;

        auto connectSet = m_connectSet;
        for (auto &item: connectSet) {
            {
                std::unique_lock<decltype(m_mutex)>     lk(m_mutex);

                auto mapIt = m_connectFunMap.find(item);
                if (mapIt == m_connectFunMap.end()) {
                    continue;
                }
                m_sharedConnect = mapIt->first;
                m_func = std::move(std::bind(mapIt->second, args ...));
            }

            auto &conn = * (Connect *) m_sharedConnect.get();
            if (! conn.alive) {
                continue;
            }

            Loop                *receiverLoop = nullptr;
            SharedLoopAlive     receiverLoopAlive = nullptr;

            if (conn.receiver) {
                std::unique_lock<decltype(conn.receiverAlive->mutex)>       lk(conn.receiverAlive->mutex);
                if (! conn.receiverAlive->alive) {
                    continue;
                }
                receiverLoop = conn.receiver->getLoop();
                receiverLoopAlive = receiverLoop->getSharedAlive();
            }
            else {
                receiverLoop = conn.receiverLoop;
            }

            if (receiverLoop == m_loop) {
                m_func();
            }
            else {
                if (conn.receiver) {
                    m_func = [=, func = std::move(m_func)]
                    {
                        if (! conn.receiverAlive->alive) {
                            return;
                        }
                        func();
                    };
                }

                switch (conn.mode)
                {
                case Connect::Auto: {
                    receiverLoop->work(std::move(m_func));
                } break;
                case Connect::Sync: {
                    receiverLoop->workSync(std::move(m_func));
                } break;
                default: {
                    receiverLoop->work(std::move(m_func));
                } break;
                }
            }

            // may delete container self, you are bad guy :(
            if (! sharedAlive->alive) {
                return;
            }
        }
    }

    void dispatchSyncHelper(const Args &... args)
    {
        auto sharedAlive = m_containerAlive;

        auto connectSet = m_connectSet;
        for (auto &item: connectSet) {
            {
                std::unique_lock<decltype(m_mutex)>     lk(m_mutex);

                auto mapIt = m_connectFunMap.find(item);
                if (mapIt == m_connectFunMap.end()) {
                    continue;
                }
                m_sharedConnect = mapIt->first;
                m_func = std::move(std::bind(mapIt->second, args ...));
            }

            auto &conn = * (Connect *) m_sharedConnect.get();
            if (! conn.alive) {
                continue;
            }

            Loop                *receiverLoop = nullptr;
            SharedLoopAlive     receiverLoopAlive = nullptr;

            if (conn.receiver) {
                std::unique_lock<decltype(conn.receiverAlive->mutex)>       lk(conn.receiverAlive->mutex);
                if (! conn.receiverAlive->alive) {
                    continue;
                }
                receiverLoop = conn.receiver->getLoop();
                receiverLoopAlive = receiverLoop->getSharedAlive();
            }
            else {
                receiverLoop = conn.receiverLoop;
            }

            if (receiverLoop == m_loop) {
                m_func();
            }
            else {
                auto w = [=, func = std::move(m_func)]
                {
                    if (! conn.receiverAlive->alive) {
                        return;
                    }
                    func();
                };

                receiverLoop->workSync(std::move(w));
            }

            // may delete container self, you are bad guy :(
            if (! sharedAlive->alive) {
                return;
            }
        }
    }

private:
    ConnectFunMap       m_connectFunMap;

    ConnectSet          m_connectSet;

    SharedConnectBase   m_sharedConnect;

    SpinMutex           m_mutex;

    std::function<void ()>      m_func;
};

// 计算函数参数个数
// 辅助结构用于计算参数数量
template<typename T, typename = void>
struct function_traits;

// 重载：针对普通函数
template<typename R, typename... Args>
struct function_traits<R(Args...)> {
    static constexpr size_t arity = sizeof...(Args);
};

// 重载：针对成员函数指针
template<typename ClassType, typename R, typename... Args>
struct function_traits<R(ClassType::*)(Args...)> {
    static constexpr size_t arity = sizeof...(Args);
};

// 重载：针对 std::function
template<typename R, typename... Args>
struct function_traits<std::function<R(Args...)>> : public function_traits<R(Args...)> {};

// 绑定函数
// 前向声明
template<class F, class Object, class Tuple, std::size_t... I>
constexpr decltype(auto) apply_bind_impl(F&& f, Object *o, Tuple&& t, std::index_sequence<I...>);

// 主函数模板
template<size_t I, class F, class Object, class Tuple>
constexpr decltype(auto) apply_bind(F&& f, Object *o, Tuple&& t)
{
    // return apply_bind_impl(std::forward<F>(f), o, std::forward<Tuple>(t),
    //                   std::make_index_sequence<std::tuple_size<std::decay_t<Tuple>>::value>{});

    return apply_bind_impl(std::forward<F>(f), o, std::forward<Tuple>(t),
                      std::make_index_sequence<I>{});
}

// 实现细节
template<class F, class Object, class Tuple, std::size_t... I>
constexpr decltype(auto) apply_bind_impl(F&& f, Object *o, Tuple&& t, std::index_sequence<I...>)
{
    return std::bind(std::forward<F>(f), o, std::get<I>(std::forward<Tuple>(t))...);
}

template<typename SenderObject, typename Signal, typename Slot>
SharedConnectBase connect(SenderObject *sender, Signal *signal, Slot slot, Connect::Mode mode = Connect::Auto)
{
    if ((size_t) signal < (size_t) sender || (size_t) signal > (size_t) sender + sizeof(SenderObject)) {
        throw std::runtime_error("signal not property of sender ?");
    }

    auto sc = SharedConnectBase(new Connect(sender, signal, nullptr, &slot, mode));

    auto senderAlive = sender->getSharedAliveMutex();
    {
        auto w = [=]
        {
            if (! senderAlive->alive) {
                return;
            }

            sender->bindSignal(signal);
            sender->connectAsSender(sc);

            signal->connect(sc, slot);

            sc->receiverLoop->addSharedConnectBase(sc);
        };

        if (sender->getLoop() == Loop::getCurrentLoop()) {
            w();
        }
        else {
            sender->getLoop()->work(std::move(w));
        }

        sc->disconnectFun = [=, receiverLoop = sc->receiverLoop] (SharedConnectBase scb)
        {
            if (! scb->alive) {
                return;
            }
            scb->alive = false;

            {
                std::unique_lock<decltype(senderAlive->mutex)>      lk(senderAlive->mutex);

                if (senderAlive->alive) {
                    sender->getLoop()->work(
                        [=]
                        {
                            if (! senderAlive->alive) {
                                return;
                            }
                            sender->removeAsSenderSharedConnect(scb);
                            signal->removeConnect(scb);
                        }
                    );
                }
            }

            receiverLoop->removeSharedConnectBase(scb);
        };
    }

    return sc;
}

template<typename Func, typename ReceiverObject, typename Slot>
struct ConnectStdFunction
{
    Func operator () (ReceiverObject *receiver, Slot &&slot)
    {
        return slot;
    }
};

template<typename Func, typename ReceiverObject, typename Slot>
struct ConnectClassFunction
{
    Func operator () (ReceiverObject *receiver, Slot &&slot)
    {
        using namespace std::placeholders;
        static const auto g_holders = std::make_tuple(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16);

        auto holders = g_holders;
        return apply_bind<function_traits<Slot>::arity>(std::forward<Slot>(slot), receiver, std::move(holders));
    }
};

template<typename F, typename SenderObject, typename Signal, typename ReceiverObject>
inline void connectDone(F &&f, SharedConnectBase sc, SenderObject *sender, Signal *signal, ReceiverObject *receiver)
{
    auto senderAlive = sender->getSharedAliveMutex();
    {
        auto w = [=, f = std::move(f)]
        {
            if (! senderAlive->alive) {
                return;
            }

            sender->bindSignal(signal);
            sender->connectAsSender(sc);

            signal->connect(sc, f);

            if (! receiver) {
                sc->receiverLoop->addSharedConnectBase(sc);
            }
        };

        if (sender->getLoop() == Loop::getCurrentLoop()) {
            w();
        }
        else {
            sender->getLoop()->work(std::move(w));
        }
    }

    if (receiver) {
        auto receiverAlive = receiver->getSharedAliveMutex();
        {
            auto w = [=]
            {
                if (! receiverAlive->alive) {
                    return;
                }

                receiver->connectAsReceiver(sc);
            };
            if (receiver->getLoop() == Loop::getCurrentLoop()) {
                w();
            }
            else {
                receiver->getLoop()->work(std::move(w));
            }
        }

        sc->disconnectFun = [=] (SharedConnectBase scb)
        {
            if (! scb->alive) {
                return;
            }
            scb->alive = false;

            {
                std::unique_lock<decltype(senderAlive->mutex)>      lk(senderAlive->mutex);

                if (senderAlive->alive) {
                    sender->getLoop()->work(
                        [=]
                        {
                            if (! senderAlive->alive) {
                                return;
                            }
                            sender->removeAsSenderSharedConnect(scb);
                            signal->removeConnect(scb);
                        }
                    );
                }
            }

            {
                std::unique_lock<decltype(receiverAlive->mutex)>    lk(receiverAlive->mutex);

                if (receiverAlive->alive) {
                    receiver->getLoop()->work(
                        [=]
                        {
                            if (! receiverAlive->alive) {
                                return;
                            }
                            receiver->removeAsReceiverSharedConnect(scb);
                        }
                    );
                }
            }
        };
    }
    else {
        sc->disconnectFun = [=, receiverLoop = sc->receiverLoop] (SharedConnectBase scb)
        {
            if (! scb->alive) {
                return;
            }
            scb->alive = false;

            {
                std::unique_lock<decltype(senderAlive->mutex)>      lk(senderAlive->mutex);

                if (senderAlive->alive) {
                    sender->getLoop()->work(
                        [=]
                        {
                            if (! senderAlive->alive) {
                                return;
                            }
                            sender->removeAsSenderSharedConnect(scb);
                            signal->removeConnect(scb);
                        }
                    );
                }
            }

            receiverLoop->removeSharedConnectBase(scb);
        };
    }
}

template<typename SenderObject, typename Signal, typename ReceiverObject, typename Slot>
struct ConnectSignalHelper
{
    SharedConnectBase operator () (SenderObject *sender, Signal *signal, ReceiverObject *receiver, Slot &&slot, Connect::Mode mode)
    {
        using Func = typename std::remove_reference<decltype(*signal)>::type::Func;
        using SlotType = typename std::remove_reference<decltype(*slot)>::type;

        if (! sender || ! signal || ! receiver || ! slot) {
            return nullptr;
        }

        auto sc = SharedConnectBase(new Connect(sender, signal, receiver, slot, mode));

        using namespace std::placeholders;
        static const auto g_holders = std::make_tuple(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16);

        auto holders = g_holders;
        Func f = apply_bind<function_traits<Func>::arity>(&SlotType::dispatch, slot, std::move(holders));

        connectDone(std::move(f), sc, sender, signal, receiver);

        return sc;
    }
};

template<typename SenderObject, typename Signal, typename ReceiverObject, typename Slot>
struct ConnectFunctionHelper
{
    SharedConnectBase operator () (SenderObject *sender, Signal *signal, ReceiverObject *receiver, Slot &&slot, Connect::Mode mode)
    {
        using Func = typename std::remove_reference<decltype(*signal)>::type::Func;

        if (! sender || ! signal) {
            return nullptr;
        }

        auto sc = SharedConnectBase(new Connect(sender, signal, receiver, &slot, mode));

        typename std::conditional<
            std::is_convertible<Slot, typename std::remove_reference<decltype(*signal)>::type::Func>::value,
            ConnectStdFunction<Func, ReceiverObject, Slot>,
            ConnectClassFunction<Func, ReceiverObject, Slot>
        >::type helper;

        Func f = helper(receiver, std::forward<Slot>(slot));

        connectDone(std::move(f), sc, sender, signal, receiver);

        return sc;
    }
};

/**
 * @brief       signal connect to slot
 * @param       sender      Object
 * @param       signal      Signal<std::function<T>>
 * @param       receiver    Object
 * @param       slot        Signal<std::function<T>> pointer, object slot, functor
 * @param       mode        Connect::Mode
 */
template<typename SenderObject, typename Signal, typename ReceiverObject, typename Slot>
SharedConnectBase connect(SenderObject *sender, Signal *signal, ReceiverObject *receiver, Slot &&slot, Connect::Mode mode = Connect::Auto)
{
    if ((size_t) signal < (size_t) sender || (size_t) signal > (size_t) sender + sizeof(SenderObject)) {
        throw std::runtime_error("signal not property of sender ?");
    }

    using Helper = typename std::conditional<
        std::is_convertible<Slot, SignalBase *>::value,
        ConnectSignalHelper<SenderObject, Signal, ReceiverObject, Slot>,
        ConnectFunctionHelper<SenderObject, Signal, ReceiverObject, Slot>
    >::type;

    // static_assert(std::is_same<Helper, ConnectFunctionHelper<SenderObject, Signal, ReceiverObject, Slot>>::value, "Helper chose first");
    // static_assert(std::is_same<Helper, ConnectSignalHelper<SenderObject, Signal, ReceiverObject, Slot>>::value, "Helper chose second");

    return Helper()(sender, signal, receiver, std::forward<Slot>(slot), mode);
}

template<typename SenderObject, typename Signal, typename Slot>
SharedConnectBase connect(SenderObject *sender, Signal *signal, nullptr_t receiver, Slot &&slot, Connect::Mode mode = Connect::Auto)
{
    return connect<SenderObject, Signal, Slot>(sender, signal, std::forward<Slot>(slot), mode);
}

};
