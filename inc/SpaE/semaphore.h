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

#include <condition_variable>
#include <mutex>

namespace SpaE
{

class Semaphore
{
public:
    Semaphore(int value = 0)
    {
        m_value = value;
    }

    void wait()
    {
        std::unique_lock<decltype(m_mutex)> lk(m_mutex);
        m_cv.wait(lk, [&] {
            return m_value > 0;
        });
        -- m_value;
    }

    bool waitFor(double sec)
    {
        std::unique_lock<decltype(m_mutex)> lk(m_mutex);
        auto ret = m_cv.wait_for(lk, std::chrono::microseconds((uint64_t) (sec * 1000000)), [&] {
            return m_value > 0;
        });
        if(ret == (bool) std::cv_status::timeout)
            -- m_value;
        return ret;
    }

    bool tryWait()
    {
        std::unique_lock<decltype(m_mutex)> lk(m_mutex);

        if(m_value > 0) {
            m_value --;

            return true;
        }
        return false;
    }

    void post()
    {
        std::unique_lock<decltype(m_mutex)> lk(m_mutex);
        ++ m_value;
        m_cv.notify_one();
    }

private:
    int m_value = 0;

    std::mutex m_mutex;

    std::condition_variable m_cv;
};

};
