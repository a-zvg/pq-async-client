#pragma once

/// @file
/// @brief 

#include <boost/stacktrace.hpp>
#include <stdexcept>
#include <sstream>
#include <string>
#include <string.h>
#include <type_traits>

enum class ErrorCode
{
    Success =       0,
    InternalError = 1,
    DataError =     2,
    FileError =     3,
    NetError =      4,
    DBMSError =     5,
};
std::error_code make_error_code( ErrorCode );

namespace std
{
template<>
struct is_error_code_enum< ErrorCode > : true_type {};
}

template< typename T, typename = std::enable_if_t< std::is_integral<T>::value > >
void f( T )
{}

class Error : public std::exception
{
public:
    template< typename T, typename... Args >
    Error( boost::stacktrace::stacktrace&& bt, const char* file, size_t line, std::error_code e, T&& reason, Args&&... args ) :
        code_( e ),
        reason_( std::forward< T >( reason ) )
    {
        params_ = { std::forward<Args>(args)... };

        std::stringstream ss;
        ss << "at " << file << ":" << line << ":\n" << bt;
        backtrace_ = ss.str();
    }
    Error() = default;

    Error( const Error& e ) = delete;
    Error& operator=( const Error& e ) = delete;
    Error( Error&& e ) = default;
    Error& operator=( Error&& e ) = default;

    const char* what() const noexcept override
    {
        return code_.category().name();
    }

    std::string What() const
    {
        std::string s = "Код " + std::to_string( code_.value() );
        s += ": " + code_.message();

        if ( !reason_.empty() )
        {
            s += ": " + reason_;
        }
        if ( !params_.empty() )
        {
            s +=  " (";
        }
        for ( auto& p : params_ )
        {
            s += "\"" + p + "\", ";
        }
        if ( !params_.empty() )
        {
            s.resize( s.size() - 2 );
            s += ')';
        }

        s += '\n' + backtrace_;
        return s;
    }

    std::error_code code_;
    std::string reason_;
    std::vector< std::string > params_;
    std::string backtrace_;
};

#define __FILENAME__ ( strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__ )
#define THROW_ERROR(...) throw Error( boost::stacktrace::stacktrace(), __FILENAME__, __LINE__, __VA_ARGS__ )
