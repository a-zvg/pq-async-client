#pragma once

/// @file
/// @brief 

#include <pq_task.h>
#include <common/error.h>
#include <common/thread.h>
#include <common/cp_queue.h>
#include <spdlog/spdlog.h>
#include <libpq-fe.h>
#include <chrono>
#include <forward_list>
#include <string>

using TaskQueue = ConsumerProducerQueue<Task>;

class PqSyncClient
{
public:
    PqSyncClient( const char* conn_str, int poolSize = 1 )
        : queue_(100)
    {
        for ( int i = 0; i < poolSize; ++i )
        {
            Thread thread( "PgSyncThread" );
            thread.Start([ this, conn_str ]
                {
                    auto conn = PQconnectdb( conn_str );
                    if ( PQstatus(conn) != CONNECTION_OK )
                    {
                        PQfinish(conn);
                        spdlog::warn( "can't connect to db \"{}\"", conn_str );
                    }

                    int pid = PQbackendPID(conn);
                    spdlog::info( "{}: connected to db \"{}\"", pid, conn_str );

                    Task task;
                    while ( queue_.WaitPop(task) )
                    {
                        PGresult* res = PQexec( conn, task.query.c_str() );
                        if ( PQresultStatus(res) != PGRES_TUPLES_OK )
                        {
                            spdlog::error( "ошибка получения результата" );
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
                    }
                } );
            threads_.push_back( std::move(thread) );
        }
    }

    ~PqSyncClient()
    {
        queue_.Disable( true );
    }

    std::future<std::string> Exec( const std::string& q )
    {
        std::promise<std::string> p;
        std::future<std::string> f = p.get_future();

        queue_.WaitPush( Task{ std::move(q), "" } );
        return f;
    }

private:
    std::vector<Thread> threads_;
    TaskQueue queue_;
};
