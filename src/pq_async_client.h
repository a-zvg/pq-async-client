#pragma once

/// @file
/// @brief 

#include <pq_async_conn.h>
#include <pq_task.h>
#include <common/error.h>
#include <common/thread.h>
#include <common/cp_queue.h>
#include <boost/asio.hpp>
#include <spdlog/spdlog.h>
#include <chrono>
#include <forward_list>
#include <deque>
#include <string>

class PqAsyncClient
{
public:
    PqAsyncClient( const char* conn_str, int poolSize = 1 )
        : thread_("PqAsyncThread")
        , queue_(100)
    {
        for ( int i = 0; i < poolSize; ++i )
        {
            pool_.emplace_front( io_context_, conn_str, queue_ );
        }

        thread_ = Thread( "PgAsyncThread", [ this ]
            {
                spdlog::debug( "Thread {} started", Thread::Name() );
                try
                {
                    io_context_.run();
                }
                catch ( const Error& e )
                {
                    spdlog::error( "{}", e.What() );
                }
                catch ( const std::exception& e )
                {
                    spdlog::error( "Непредвиденная ошибка: \"{}\"", e.what() );
                }
            } );
        thread_.Start();
    }

    ~PqAsyncClient()
    {
        thread_.Stop( [ this ] { io_context_.stop(); } );
    }

    PqResult Exec( ba::io_context& io_context, const std::string& query )
    {
        bool ready = std::any_of( std::cbegin(pool_), std::cend(pool_), []( auto& dbConn )
             {
                 return dbConn.GetState() != PqAsyncConn::State::None;
             } );

        if ( !ready || !queue_.WaitPush( PqResult( io_context, query ) ) )
        {
            PqResult res( io_context, query );
            res.SetError( ECANCELED );
            return res;
        }
    }

    std::future<std::string> Exec( const std::string& q )
    {
        bool ready = std::any_of( std::cbegin(pool_), std::cend(pool_), []( auto& dbConn )
             {
                 return dbConn.GetState() != PqAsyncConn::State::None;
             } );

        std::promise<std::string> p;
        std::future<std::string> f = p.get_future();

        if ( !ready || !queue_.WaitPush( Task{ std::move(q), "" } ) )
        {
            //std::promise<std::string> p;
            //f = p.get_future();
            try
            {
                throw std::runtime_error( "not ready" );
            }
            catch( std::exception& e )
            {
                //p.set_exception( std::current_exception() );
            }
        }
        else if ( std::any_of( std::cbegin(pool_), std::cend(pool_), []( auto& dbConn )
                {
                    return dbConn.GetState() == PqAsyncConn::State::Idle;
                } ) )
        {
            ba::post( io_context_, [ this, q = std::move(q) ]() mutable
                {
                    auto it = std::find_if( std::begin(pool_), std::end(pool_), []( auto& dbConn )
                        {
                            return dbConn.GetState() == PqAsyncConn::State::Idle;
                        } );
                    if ( it != std::end(pool_) )
                    {
                        it->Process();
                    }
                } );
        }
        return f;
    }

private:
    ba::io_context io_context_;
    Thread thread_;
    std::forward_list<PqAsyncConn> pool_;
    TaskQueue queue_;
};
