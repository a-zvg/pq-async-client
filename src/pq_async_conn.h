#pragma once

/// @file
/// @brief 

#include <pq_task.h>
#include <common/error.h>
#include <common/cp_queue.h>
#include <boost/asio.hpp>
#include <spdlog/spdlog.h>
#include <libpq-fe.h>
#include <deque>
#include <string>

namespace ba = boost::asio;
using ba::ip::tcp;
using TaskQueue = ConsumerProducerQueue<Task>;

class PqAsyncConn
{
public:
    enum class State
    {
        None,
        Idle,
        Busy
    };

    PqAsyncConn( ba::io_context& io_context, const char* conn_str, TaskQueue& queue ) :
        io_context_(io_context),
        connStr_(conn_str),
        socket_(io_context),
        queue_(queue),
        timer_( io_context, ba::chrono::seconds(0) )
    {
        CheckConn();
    }

    ~PqAsyncConn()
    {
        if (conn_)
        {
            PQfinish(conn_);
        }
    }

    State GetState() const
    {
        return state_.load();
    }

    void Process()
    {
        Task task;
        if ( !queue_.TryPop( task, std::chrono::seconds(0) ) )
        {
            return;
        }

        //spdlog::info( "{}: \"{}\"", pid_, task.query );

        if ( !PQsendQuery( conn_, task.query.c_str() ) )
        {
            spdlog::error( "{}: db query error", pid_ );
            Process();
        }
        else
        {
            state_.store( State::Busy );
            socket_.async_wait( tcp::socket::wait_read, [ this, task = std::move(task) ]( boost::system::error_code ec ) mutable
               {
                   if ( !ec )
                   {
                       GetResult( std::move(task) );
                   }
                   else
                   {
                       try
                       {
                           THROW_ERROR( ErrorCode::DBMSError, "ошибка получения результата" );
                       }
                       catch (...)
                       {
                           //task.promise.set_exception( std::current_exception() );
                           spdlog::error( "ошибка получения результата" );
                       }
                       Process();
                   }
               } );
        }
    }
private:
    ba::io_context& io_context_;
    std::string connStr_;
    PGconn* conn_ = NULL;
    tcp::socket socket_;
    int pid_ = 0;
    TaskQueue& queue_;
    ba::steady_timer timer_;
    std::atomic<State> state_{ State::None };

    void CheckConn()
    {
        timer_.async_wait( [ this ]( boost::system::error_code ec )
            {
                if ( !ec )
                {
                    if ( GetState() == State::None )
                    {
                        ConnectDB();
                    }
                    else if ( GetState() == State::Idle && !PQconsumeInput( conn_ ) )
                    {
                        spdlog::warn( "{}: db connection lost", pid_ );
                        ConnectDB();
                    }
                    timer_.expires_after( ba::chrono::seconds(1) );
                    CheckConn();
                }
                else if ( ec != boost::system::errc::operation_canceled )
                {
                    THROW_ERROR( ErrorCode::InternalError,  ec.message() );
                }
            } );
    }

    void ConnectDB()
    {
        if (conn_)
        {
            state_.store( State::None );
            socket_.release();
            PQfinish(conn_);
            conn_ = NULL;
        }

        conn_ = PQconnectdb( connStr_.c_str() ); // todo реализовать ассинхронное подключение
        if ( PQstatus(conn_) != CONNECTION_OK )
        {
            PQfinish(conn_);
            conn_ = NULL;
            spdlog::warn( "can't connect to db \"{}\"", connStr_ );
        }
        else
        {
            state_.store( State::Idle );
            socket_.assign( tcp::socket::protocol_type::v4(), PQsocket(conn_) );
            pid_ = PQbackendPID(conn_);
            spdlog::info( "{}: connected to db \"{}\"", pid_, connStr_ );
        }
    }

    void GetResult( Task task )
    {
        PQconsumeInput(conn_);

        if ( PQisBusy(conn_) ) {
            ba::post( io_context_, [ this, task = std::move(task) ]() mutable { GetResult( std::move(task) ); } );
            return;
        }

        PGresult* res = PQgetResult(conn_);
        if ( res == nullptr )
        {
            //task.promise.set_value( task.result );
            if ( queue_.Empty() )
            {
                state_.store( State::Idle );
            }
            else
            {
                Process();
            }
            return;
        }

        if ( PQresultStatus(res) != PGRES_TUPLES_OK )
        {
            try
            {
                THROW_ERROR( ErrorCode::DBMSError, "ошибка получения результата" );
            }
            catch (...)
            {
                //task.promise.set_exception( std::current_exception() );
                spdlog::error( "ошибка получения результата" );
            }
            PQclear(res);
            while ( PQgetResult(conn_) )
            {
                PQclear(res);
            }
            Process();
            return;
        }
        else
        {
            std::stringstream ss;
            for ( int i = 0; i < PQntuples(res); ++i )
            {
                for ( int j = 0; j < PQnfields(res); ++j )
                {
                    ss << PQgetvalue(res, i, j) << "|";
                }
                ss << '\n';
                task.result += ss.str();
            }
        }
        PQclear(res);

        ba::post( io_context_, [ this, task = std::move(task) ]() mutable { GetResult( std::move(task) ); } );
    }
};
