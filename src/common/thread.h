#pragma once

/// @file
/// @brief Файл с объявлением класса потока исполнения

#include <functional>
#include <thread>

/// @brief Класс потока исполнения
class Thread
{
public:
    /// @brief Конструктор
    /// @param name имя потока
    Thread( const std::string& name ) : name_(name)
    {
    }

    /// @brief Конструктор
    /// @param name имя потока
    /// @param f функция исполнения
    Thread( const std::string& name, std::function< void() > f ) :
        name_(name),
        f_( std::move(f) )
    {
    }

    /// @brief Деструктор
    ~Thread()
    {
        WaitStop();
    }

    Thread( Thread&& ) = default;
    Thread& operator=( Thread&& ) = default;

    /// @brief Запустить поток
    void Start()
    {
        if ( t_.joinable() ) // на случай перезапуска потока, который еще не завершился из-за команды асинхронной остановки
        {
            t_.join();
        }
        if ( !f_ )
        {
            throw std::bad_function_call();
        }
        t_ = std::thread( []( std::string name, std::function< void() > f )
            {
                Thread::Name( name );
                f();
            }, name_, f_ );
    }

    /// @brief Запустить поток
    /// @param f функция исполнения
    void Start( std::function< void() > f )
    {
        f_ = std::move(f);
        Start();
    }

    /// @brief Остановить поток
    void WaitStop()
    {
        if ( t_.joinable() )
        {
            t_.join();
        }
    }

    /// @brief Остановить поток
    /// @tparam Func тип функции остановки
    /// @param func функция остановки потока
    /// @param sync остановить, дожидаясь реальной остановки или нет
    template< typename Func >
    void Stop( Func&& func, bool sync = false )
    {
        func();
        if ( sync )
        {
            WaitStop();
        }
    }

    /// @brief Проверить запущен ли поток
    /// @return true, если запущен или false, если нет
    bool IsStarted() const
    {
        return t_.joinable();
    }

    /// @brief Получить/установить имя потока
    /// @return имя потока
    static std::string Name( const std::string& name = {} )
    {
        thread_local std::string threadName;
        if ( !name.empty() )
        {
            threadName = name;
        }
        return threadName;
    }

protected:
    std::string name_;          ///< Имя потока
    std::function< void() > f_; ///< Функция исполнения потока
    std::thread t_;             /// Объект потока
};
