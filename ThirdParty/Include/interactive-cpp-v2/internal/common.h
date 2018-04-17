#pragma once

#include <string>
#include <memory>

#define RETURN_IF_FAILED(x) do { int __err_c = x; if(0 != __err_c) return __err_c; } while(0)

namespace mixer_internal
{

// String conversion functions
std::wstring utf8_to_wstring(const std::string& str);
std::string wstring_to_utf8(const std::wstring& wstr);

#if _DURANGO || defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PC_APP
#elif _WIN32
// WinHttp common

struct hinternet_deleter
{
	void operator()(void* internet);
};

typedef std::unique_ptr<void, hinternet_deleter> hinternet_ptr;

#endif

}