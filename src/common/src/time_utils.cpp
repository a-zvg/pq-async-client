/// @file
/// @brief 

#include <common/error.h>
#include <common/time_utils.h>
#include <iomanip>
#include <stdexcept>
#include <string>

TimePoint GetTime( const std::string& dateTime, const std::string& format )
{
    std::stringstream ss;
    ss << dateTime;

    std::tm tm = {};
    ss >> std::get_time( &tm, format.c_str() );
    if ( ss.fail() )
    {
        THROW_ERROR( ErrorCode::DataError, "некорректные", dateTime, format );
    }
    return std::chrono::system_clock::from_time_t( std::mktime( &tm ) - timezone );
}

std::string PutTime( TimePoint tp, const std::string& format )
{
    std::time_t t = std::chrono::system_clock::to_time_t( tp );
    std::tm tm = {};
    if ( !gmtime_r( &t, &tm ) )
    {
        THROW_ERROR( ErrorCode::DataError, "некорректные", std::to_string( t ), format );
    }

    std::stringstream ss;
    ss << std::put_time( &tm, format.c_str() );
    return ss.str();
}
