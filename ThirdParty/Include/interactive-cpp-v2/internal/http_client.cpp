#include "http_client.h"

#if _WIN32
#include "win_http_client.h"
#endif
namespace mixer
{
std::unique_ptr<http_client>
http_factory::make_http_client()
{
#if _WIN32
	return std::make_unique<win_http_client>();
#else
#error "Missing http implementation for this platform."
#endif
}
}