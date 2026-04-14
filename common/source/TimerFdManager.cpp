#include "TimerFdManager.h"

#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <unistd.h>

namespace firebolt::rialto::common
{

static timespec toTimespec(std::chrono::milliseconds ms)
{
    timespec ts{};
    ts.tv_sec = ms.count() / 1000;
    ts.tv_nsec = (ms.count() % 1000) * 1000000;
    return ts;
}

TimerFdManager &TimerFdManager::instance()
{
    static TimerFdManager inst;
    return inst;
}

TimerFdManager::TimerFdManager()
{
    m_epoll = epoll_create1(EPOLL_CLOEXEC);
    m_thread = std::thread(&TimerFdManager::loop, this);
}

TimerFdManager::~TimerFdManager()
{
    m_running = false;
    close(m_epoll);
    if (m_thread.joinable())
        m_thread.join();
}

TimerId TimerFdManager::add(std::chrono::milliseconds timeout,
                            TimerType type,
                            std::function<void()> cb)
{
    int fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);

    itimerspec spec{};
    spec.it_value = toTimespec(timeout);
    spec.it_interval =
        (type == TimerType::PERIODIC) ? toTimespec(timeout) : timespec{};

    timerfd_settime(fd, 0, &spec, nullptr);

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    epoll_ctl(m_epoll, EPOLL_CTL_ADD, fd, &ev);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_callbacks.emplace(fd, std::move(cb));
    }

    return fd;
}

void TimerFdManager::cancel(TimerId id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_callbacks.find(id);
    if (it == m_callbacks.end())
        return;

    epoll_ctl(m_epoll, EPOLL_CTL_DEL, id, nullptr);
    close(id);
    m_callbacks.erase(it);
}

void TimerFdManager::loop()
{
    epoll_event ev[8];
    while (m_running)
    {
        int n = epoll_wait(m_epoll, ev, 8, -1);
        if (n <= 0)
            break;

        for (int i = 0; i < n; ++i)
        {
            uint64_t exp;
            read(ev[i].data.fd, &exp, sizeof(exp));

            std::function<void()> cb;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                auto it = m_callbacks.find(ev[i].data.fd);
                if (it != m_callbacks.end())
                    cb = it->second;
            }
            if (cb)
                cb();
        }
    }
}

} // namespace firebolt::rialto::common
