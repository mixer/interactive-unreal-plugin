#include "http_client.h"

#if _DURANGO || defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PC_APP
#include "winapp_http_client.h"
#elif _WIN32
#include "win_http_client.h"
#endif
namespace mixer
{
std::unique_ptr<http_client>
http_factory::make_http_client()
{
#if _DURANGO || defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PC_APP
	return std::make_unique<winapp_http_client>();
#elif _WIN32
	return std::make_unique<win_http_client>();
#else
#error "Missing http implementation for this platform."
#endif
}
}