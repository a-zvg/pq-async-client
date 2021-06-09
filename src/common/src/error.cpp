/// @file
/// @brief 

#include <common/error.h>

namespace
{

struct ErrorCategory : std::error_category
{
    const char* name() const noexcept override
    {
        return "Ошибка";
    }

    std::string message( int ev ) const override
    {
        switch ( static_cast< ErrorCode >( ev ) )
        {
            case ErrorCode::DataError:
                return "Ошибка данных";
            case ErrorCode::FileError:
                return "Ошибка файла";
            case ErrorCode::NetError:
                return "Ошибка сетевого взаимодействия";
            case ErrorCode::DBMSError:
                return "Ошибка работы с СУБД";
            default:
                return "Внутренняя ошибка";
        }
    }
};
const ErrorCategory errorCategory {};

} //namespace

std::error_code make_error_code( ErrorCode e )
{
    return { static_cast< int >( e ), errorCategory };
}
