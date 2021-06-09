#pragma once

/// @file
/// @brief 

#include <chrono>
#include <string>

using TimePoint = std::chrono::time_point< std::chrono::system_clock >;

TimePoint GetTime( const std::string& dateTime, const std::string& format = "%d.%m.%Y %H:%M:%S" );

std::string PutTime( TimePoint tp, const std::string& format = "%d.%m.%Y %H:%M:%S" );
