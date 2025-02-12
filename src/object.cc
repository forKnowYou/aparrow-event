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

#include <SpaE/object.h>

#include <stdexcept>

#include <SpaE/spin_mutex.h>

using namespace SpaE;

static std::atomic<ConnectId>   g_connectId { 1 };

static std::atomic<ObjectId>    g_objectId { 1 };

void SignalBase::bindContainer(Object *o, Loop *loop)
{
    m_containerAlive = o->getSharedAliveMutex();

    m_loop = loop;
}

Connect::Connect()
{
    throw std::invalid_argument("Invalid use of SpaE::Connect, no constructor arguments");
}

Connect::Connect(Object *sender, SignalBase *signal, Object *receiver, void *slot, const Mode mode)
{
    this->sender = sender;
    this->signal = signal;
    this->senderId = sender->getId();
    this->senderAlive = sender->getSharedAliveMutex();
    this->receiver = receiver;
    this->receiverId = 0;
    if (receiver) {
        this->receiverId = receiver->getId();
        this->receiverAlive = receiver->getSharedAliveMutex();
    }
    else {
        this->receiverLoop = Loop::getCurrentLoop();
        this->receiverLoopAlive = this->receiverLoop->getSharedAlive();
    }
    this->slot = slot;
    this->mode = mode;
    this->id = g_connectId ++;
    this->alive = true;
}

Object::Object()
{
    m_loop = Loop::getCurrentLoop();
    m_loopAlive = m_loop->getSharedAlive();
    m_id = g_objectId ++;
    m_aliveMutex = std::make_shared<AliveMutex>();
}

Object::Object(const Object &other)
{
    m_loop = Loop::getCurrentLoop();
    m_loopAlive = m_loop->getSharedAlive();
    m_id = g_objectId ++;
    m_aliveMutex = std::make_shared<AliveMutex>();
}

Object &Object::operator = (const Object &other)
{
    m_loop = Loop::getCurrentLoop();
    m_loopAlive = m_loop->getSharedAlive();
    m_id = g_objectId ++;
    m_aliveMutex = std::make_shared<AliveMutex>();

    return *this;
}

Object::~Object()
{
    {
        std::unique_lock<decltype(m_aliveMutex->mutex)>     lk(m_aliveMutex->mutex);

        m_aliveMutex->alive = false;
    }

    auto curr = Loop::getCurrentLoop();
    if (Loop::getCurrentLoop() != m_loop) {
        fprintf(stderr,
            "warning: SpaE::Object::%s() in another thread. (curr=%s, loop=%s) \r\n",
            __FUNCTION__, curr->getName(), m_loop->getName()
        );
    }

    for (auto &it: m_asSenderConnectSet) {
        auto &conn = * (Connect *) it.get();

        conn.disconnectFun(it);
    }
    for (auto &it: m_asReceiverConnectSet) {
        auto &conn = * (Connect *) it.get();

        conn.disconnectFun(it);
    }
}

ObjectId Object::getId()
{
    return m_id;
}

Loop *Object::getLoop()
{
    return m_loop;
}

SharedLoopAlive Object::getLoopSharedAlive()
{
    return m_loopAlive;
}

SharedAliveMutex Object::getSharedAliveMutex()
{
    return m_aliveMutex;
}

void Object::bindSignal(SignalBase *signal)
{
    signal->bindContainer(this, m_loop);

    m_signalSet.emplace(signal);
}

void Object::connectAsSender(const SharedConnectBase &scb)
{
    m_asSenderConnectSet.emplace(scb);
}

void Object::connectAsReceiver(const SharedConnectBase &scb)
{
    m_asReceiverConnectSet.emplace(scb);
}

void Object::disconnect(const ObjectId &receiverId)
{
    for (auto &it: m_asSenderConnectSet ) {
        auto &conn = * (Connect *) it.get();
        if (conn.receiverId != receiverId) {
            // rm it at disconnectFun
            conn.disconnectFun(it);
            continue;
        }
    }
}

void Object::disconnect(SignalBase *signal, const ObjectId &receiverId, void *slot)
{
    if ((signal) && (! slot)) {
        for (auto &it: m_asSenderConnectSet ) {
            auto &conn = * (Connect *) it.get();
            if (conn.signal == signal && conn.receiverId == receiverId) {
                // rm it at disconnectFun
                conn.disconnectFun(it);
                continue;
            }
        }
    }
    else if ((! signal) && (slot)) {
        for (auto &it: m_asSenderConnectSet ) {
            auto &conn = * (Connect *) it.get();
            if (conn.receiverId == receiverId && conn.slot == slot) {
                // rm it at disconnectFun
                conn.disconnectFun(it);
                continue;
            }
        }
    }
    else if ((signal) && (slot)) {
        for (auto &it: m_asSenderConnectSet ) {
            auto &conn = * (Connect *) it.get();
            if (conn.signal == signal && conn.receiverId == receiverId && conn.slot == slot) {
                // rm it at disconnectFun
                conn.disconnectFun(it);
                continue;
            }
        }
    }
}

void Object::disconnect(SignalBase *signal)
{
    if (signal) {
        for (auto &it: m_asSenderConnectSet ) {
            auto &conn = * (Connect *) it.get();
            if (conn.signal != signal) {
                // rm it at disconnectFun
                conn.disconnectFun(it);
                continue;
            }
        }
    }
    else {
        for (auto &it: m_asSenderConnectSet) {
            auto &conn = * (Connect *) it.get();
            // rm it at disconnectFun
            conn.disconnectFun(it);
        }
    }
}

void Object::disconnect(void *slot)
{
    if (slot) {
        for (auto &it: m_asReceiverConnectSet ) {
            auto &conn = * (Connect *) it.get();
            if (conn.slot != slot) {
                // rm it at disconnectFun
                conn.disconnectFun(it);
                continue;
            }
        }
    }
    else {
        for (auto &it: m_asReceiverConnectSet) {
            auto &conn = * (Connect *) it.get();
            // rm it at disconnectFun
            conn.disconnectFun(it);
        }
    }
}

void Object::disconnectSender(const ObjectId &senderId, SignalBase *signal)
{
    if (! signal) {
        for (auto &it: m_asReceiverConnectSet) {
            auto &conn = * (Connect *) it.get();
            if (conn.senderId == senderId) {
                // rm it at disconnectFun
                conn.disconnectFun(it);
                continue;
            }
        }
    }
    else {
        for (auto &it: m_asReceiverConnectSet) {
            auto &conn = * (Connect *) it.get();
            if (conn.senderId == senderId && conn.signal == signal) {
                // rm it at disconnectFun
                conn.disconnectFun(it);
                continue;
            }
        }
    }
}

void Object::disconnectReceiver(const ObjectId &receiverId, void *slot)
{
    if (! slot) {
        for (auto &it: m_asSenderConnectSet) {
            auto &conn = * (Connect *) it.get();
            if (conn.receiverId == receiverId) {
                // rm it at disconnectFun
                conn.disconnectFun(it);
                continue;
            }
        }
    }
    else {
        for (auto &it: m_asSenderConnectSet) {
            auto &conn = * (Connect *) it.get();
            if (conn.receiverId == receiverId && conn.slot == slot) {
                // rm it at disconnectFun
                conn.disconnectFun(it);
                continue;
            }
        }
    }
}

void Object::removeAsSenderSharedConnect(const SharedConnectBase &scb)
{
    auto it = m_asSenderConnectSet.find(scb);
    if (it == m_asSenderConnectSet.end()) {
        return;
    }
    m_asSenderConnectSet.erase(it);
}

void Object::removeAsReceiverSharedConnect(const SharedConnectBase &scb)
{
    auto it = m_asReceiverConnectSet.find(scb);
    if (it == m_asReceiverConnectSet.end()) {
        return;
    }
    m_asReceiverConnectSet.erase(it);
}

void Object::moveToLoop(Loop *loop)
{
    auto curr = Loop::getCurrentLoop();
    if (Loop::getCurrentLoop() != m_loop) {
        fprintf(stderr,
            "warning: SpaE::Object::%s() in another thread. (curr=%s, loop=%s) \r\n",
            __FUNCTION__, curr->getName(), m_loop->getName()
        );
    }

    for (auto &it: m_signalSet) {
        it->bindContainer(this, loop);
    }

    m_loop = loop;
    m_loopAlive = loop->getSharedAlive();
}



void SpaE::disconnect(const SharedConnectBase &sc)
{
    sc->disconnectFun(sc);
}

void SpaE::disconnect(Object *sender, Object *receiver)
{
    auto senderAlive = sender->getSharedAliveMutex();
    auto receiverId = receiver->getId();

    if (sender->getLoop() == Loop::getCurrentLoop()) {
        sender->disconnect(receiverId);
    }
    else {
        sender->getLoop()->work(
            [=]
            {
                if (! senderAlive->alive) {
                    return;
                }

                sender->disconnect(receiverId);
            }
        );
    }
}

void SpaE::disconnect(Object *sender, SignalBase *signal, Object *receiver, void *slot)
{
    if ((! sender) && (signal)) {
        return;
    }
    if ((! receiver) && (slot)) {
        return;
    }
    if ((sender) && (! receiver) && (! slot)) {
        disconnectAsSender(sender, signal);
        return;
    }
    if ((! sender) && (! signal) && (receiver)) {
        disconnectAsReceiver(receiver, slot);
        return;
    }
    if ((sender) && (! signal) && (receiver) && (! slot)) {
        disconnect(sender, receiver);
        return;
    }

    auto senderAlive = sender->getSharedAliveMutex();
    auto receiverId = receiver->getId();

    if (sender->getLoop() == Loop::getCurrentLoop()) {
        sender->disconnect(signal, receiverId, slot);
    }
    else {
        sender->getLoop()->work(
            [=]
            {
                if (! senderAlive->alive) {
                    return;
                }

                sender->disconnect(signal, receiverId, slot);
            }
        );
    }
}

void SpaE::disconnectAsSender(Object *sender, SignalBase *signal)
{
    auto senderAlive = sender->getSharedAliveMutex();

    if (sender->getLoop() == Loop::getCurrentLoop()) {
        sender->disconnect(signal);
    }
    else {
        sender->getLoop()->work(
            [=]
            {
                if (! senderAlive->alive) {
                    return;
                }

                sender->disconnect(signal);
            }
        );
    }
}

void SpaE::disconnectAsReceiver(Object *receiver, void *slot)
{
    auto receiverAlive = receiver->getSharedAliveMutex();

    if (receiver->getLoop() == Loop::getCurrentLoop()) {
        receiver->disconnect(slot);
    }
    else {
        receiver->getLoop()->work(
            [=]
            {
                if (! receiverAlive->alive) {
                    return;
                }

                receiver->disconnect(slot);
            }
        );
    }
}
