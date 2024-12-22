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

#include <stdint.h>

#include <unordered_map>
#include <unordered_set>
#include <memory>

#include "loop.h"

namespace SpaE
{

class SignalBase;

struct Connect;

using SharedConnect = std::shared_ptr<Connect>;

class Object;

using ObjectId = uint64_t;

class SignalBase
{
public:
    virtual ~SignalBase()
    {

    }

    virtual void removeConnect(const SharedConnectBase &conn) = 0;

    void bindContainer(Object *o, Loop *loop);

protected:
    Loop                *m_loop;

    SharedAliveMutex    m_containerAlive;
};

struct Connect : public ConnectBase
{
    enum Mode {
        Auto,
        Sync,
    } mode;

    Object      *sender,
                *receiver;

    ObjectId    senderId,
                receiverId;

    SharedAliveMutex    senderAlive,
                        receiverAlive;

    SignalBase  *signal;

    void        *slot;

    Connect();
    Connect(Object *sender, SignalBase *signal, Object *receiver, void *slot, const Mode mode = Auto);
};

class Object
{
public:
    friend class Connect;

    using SignalSet = std::unordered_set<SignalBase *>;

    Object();

    Object(const Object &other);

    Object &operator = (const Object &other);

    ~Object();

    ObjectId    getId();

    Loop        *getLoop();

    SharedLoopAlive         getLoopSharedAlive();
    SharedAliveMutex        getSharedAliveMutex();

    void        bindSignal(SignalBase *signal);

    void        moveToLoop(Loop *loop);

    void        connectAsSender(const SharedConnectBase &scb);
    void        connectAsReceiver(const SharedConnectBase &scb);

    void        disconnect(const ObjectId &receiverId);
    void        disconnect(SignalBase *signal, const ObjectId &receiverId, void *slot);
    void        disconnect(SignalBase *signal);
    void        disconnect(void *slot);
    void        disconnectSender(const ObjectId &senderId, SignalBase *signal);
    void        disconnectReceiver(const ObjectId &receiverId, void *slot);

    void        removeAsSenderSharedConnect(const SharedConnectBase &scb);
    void        removeAsReceiverSharedConnect(const SharedConnectBase &scb);

private:
    Loop    *m_loop;

    SharedLoopAlive         m_loopAlive;

    SharedAliveMutex        m_aliveMutex;

    ObjectId        m_id;

    SharedConnectBaseSet    m_asSenderConnectSet, m_asReceiverConnectSet;

    SignalSet       m_signalSet;
};

void disconnect(const SharedConnect &sc);
void disconnect(Object *sender, Object *receiver);
void disconnect(Object *sender, SignalBase *signal, Object *receiver, void *slot);
void disconnectAsSender(Object *sender, SignalBase *signal = nullptr);
void disconnectAsReceiver(Object *receiver, void *slot = nullptr);

};

#ifndef QT_VERSION
#define emit
#define signals     public
#define slots       public
#endif
