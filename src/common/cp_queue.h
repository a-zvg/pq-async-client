#pragma once

/// @file
/// @brief Файл с объявлением многопоточной очереди

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>

using namespace std::chrono_literals;

/// @brief Класс многопоточной очереди
/// @tparam T тип элементов очереди
template<typename T>
class ConsumerProducerQueue
{
public:
    /// @brief Конструктор
    /// @param maxSize максимальный размер очереди
    /// @note maxSize = 0 означает, что очередь почти неограниченнго размера
    ConsumerProducerQueue(std::size_t maxSize = 0) : maxSize_{maxSize}
    {
        if (maxSize_.load() == 0)
        {
            maxSize_--;
        }
    }

    /// @brief Вставить элемент в конец очереди с ожидаем, если очередь переполнена
    /// @tparam U тип элемента
    /// @param item элемент
    /// @return true, если элемент вставлен или false, если очередь заблокирована
    template<typename U>
    bool WaitPush(U&& item)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (queue_.size() >= maxSize_.load() && !disabled_.load())
        {
            cond_.wait(lock);
        }
        if (disabled_.load())
        {
            return false;
        }
        queue_.push(std::forward<U>(item));
        lock.unlock();

        cond_.notify_all();
        return true;
    }

    /// @brief Изъять первый элемент из очереди с ожиданием, если очередь пуста
    /// @param item элемент
    /// @return true, если элемент получен или false, если очередь заблокирована
    bool WaitPop(T& item)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (queue_.empty() && !disabled_.load())
        {
            cond_.wait(lock);
        }
        if (queue_.empty())
        {
            return false;
        }
        item = queue_.front();
        queue_.pop();
        lock.unlock();

        cond_.notify_all();
        return true;
    }

    /// @brief Вставить элемент в конец очереди с ожидаем по таймауту
    /// @tparam Rep тип количества тиков
    /// @tparam Period тип количества секунд на тик
    /// @tparam U тип элемента
    /// @param item элемент
    /// @param timeout таймаут ожидания добавления элемента
    /// @return true, если элемент вставлен или false, если истек таймаут или очередь заблокирована
    template<typename U, typename Rep, typename Period>
    bool TryPush(U&& item, std::chrono::duration<Rep, Period> timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (queue_.size() >= maxSize_.load() && !disabled_.load())
        {
            if (cond_.wait_for(lock, timeout) == std::cv_status::timeout)
            {
                return false;
            }
        }
        if (disabled_.load())
        {
            return false;
        }
        queue_.push(std::forward<U>(item));
        lock.unlock();

        cond_.notify_all();
        return true;
    }

    /// @brief Изъять первый элемент из очереди с ожиданием по таймауту
    /// @tparam Rep тип количества тиков
    /// @tparam Period тип количества секунд на тик
    /// @param item элемент
    /// @param timeout таймаут ожидания получения элемента
    /// @return true, если элемент получен или false, если истек таймаут или очередь заблокирована
    template<typename Rep, typename Period>
    bool TryPop(T& item, std::chrono::duration<Rep, Period> timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (queue_.empty() && !disabled_.load())
        {
            if (cond_.wait_for(lock, timeout) == std::cv_status::timeout)
            {
                return false;
            }
        }
        if (queue_.empty())
        {
            return false;
        }
        item = queue_.front();
        queue_.pop();
        lock.unlock();

        cond_.notify_all();
        return true;
    }

    /// @brief Изменить максимальный размер очереди
    /// @param maxSize максимальный размер очереди
    void SetMaxSize(std::size_t maxSize)
    {
        maxSize_.store(maxSize);
    }

    /// @brief Заблокировать очередь
    /// @param force заблокировать с очищением имеющихся элементов
    void Disable(bool force = false)
    {
        disabled_.store(true);
        if (force)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            while (!queue_.empty())
            {
                queue_.pop();
            }
        }
        cond_.notify_all();
    }

    /// @brief Проверить заблокирована ли очередь
    /// @return true, если заблокирована или false, если нет
    bool IsDisabled() const
    {
        return disabled_.load();
    }

    /// @brief Проверить отсутствие элементов в очереди
    /// @return true, если элементов нет или false, если есть
    bool Empty()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    size_t Size()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    std::queue<T> queue_;
    std::atomic<std::size_t> maxSize_{0};
    std::condition_variable cond_;
    std::mutex mutex_;
    std::atomic_bool disabled_{false};
};
