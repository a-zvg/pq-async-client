#pragma once

/// @file
/// @brief 

#include <common/error.h>
#include <boost/asio.hpp>
#include <future>
#include <string>
#include <system_error>

struct Task
{
    std::string query;
    //std::promise<std::string> promise;
    std::string result;
};

class PqResult
{
public:
    template< typename T>
    PqResult( boost::asio::io_context& io_context, T&& query )
        : query_( std::forward<T>(query) )
        , timer_(io_context)
    {
        timer_.expires_at( boost::posix_time::pos_infin );
    }

    PqResult( PqResult& ) = delete;
    PqResult& operator=( const PqResult& ) = delete;
    PqResult( PqResult&& ) = default;
    PqResult& operator=( PqResult&& ) = default;

    void SetError( int errn )
    {
        //ec_ = std::make_error_code( std::errc::operation_canceled );
        ec_ = { errn, std::system_category() };
    }

private:
    std::string query_;
    boost::asio::deadline_timer timer_;
    std::string result_;
    std::error_code ec_;
};
