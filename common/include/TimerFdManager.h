#include <chrono>
#include <functional>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <mutex>
#include "ITimer.h"

namespace firebolt::rialto::common
{

using TimerId = int;

class TimerFdManager
{
public:
    static TimerFdManager &instance();

    TimerId add(std::chrono::milliseconds timeout,
                TimerType type,
                std::function<void()> cb);

    void cancel(TimerId id);

private:
    TimerFdManager();
    ~TimerFdManager();
    void loop();

    int m_epoll{-1};
    std::thread m_thread;
    std::atomic<bool> m_running{true};

    std::mutex m_mutex;
    std::unordered_map<TimerId, std::function<void()>> m_callbacks;
};

} // namespace firebolt::rialto::common

