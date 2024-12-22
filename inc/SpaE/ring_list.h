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

#include <vector>

// for one-write-one-read operator
template<typename T>
class ring_list {
public:
    using __T = T;

    struct Item {
        T t;
        Item* next;
    };

public:
    ring_list() {
        newItems();
    }

    void push(const T &t) {
        if (m_available == m_size) {
            newItems();
        }
        m_itemWrite->t = t;
        m_itemWrite = m_itemWrite->next;

        m_available ++;
    }

    void push(T&& t) {
        if (m_available == m_size) {
            newItems();
        }
        m_itemWrite->t = std::move(t);
        m_itemWrite = m_itemWrite->next;

        m_available ++;
    }

    int available() {
        return m_available;
    }

    int pop(T &t) {
        if (! m_available) {
            return 0;
        }
        t = std::move(m_itemRead->t);

        m_itemRead = m_itemRead->next;

        m_available --;

        return 1;
    }

protected:
    void newItems() {
        std::vector<Item> items(64);
        for (size_t i = 0; i < items.size() - 1; i++) {
            items[i].next = &items[i + 1];
        }

        if (!m_allocated.size()) {
            m_itemWrite = &items[0];
            m_itemRead = m_itemWrite;
            items.rbegin()->next = &items[0];
        }
        else {
            m_allocated.rbegin()->rbegin()->next = &items[0];
            items.rbegin()->next = &m_allocated[0][0];
        }

        m_allocated.emplace_back(std::move(items));

        m_size += items.size();
    }

protected:
    int m_available = 0;
    int m_size = 0;
    Item* m_itemWrite = nullptr;
    Item* m_itemRead = nullptr;
    std::vector<std::vector<Item>> m_allocated;
};
