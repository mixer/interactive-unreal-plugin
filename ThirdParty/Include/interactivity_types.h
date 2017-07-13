
#ifdef _WIN32
#include <Windows.h>
#include <system_error>

#ifndef _WIN32_WINNT_WIN10
#define _WIN32_WINNT_WIN10 0x0A00
#endif

#ifndef TV_API
#define TV_API (WINAPI_FAMILY == WINAPI_FAMILY_TV_APP || WINAPI_FAMILY == WINAPI_FAMILY_TV_TITLE) 
#endif

#ifndef UWP_API
#define UWP_API (WINAPI_FAMILY == WINAPI_FAMILY_APP && _WIN32_WINNT >= _WIN32_WINNT_WIN10)
#endif

#include <cpprest/http_client.h>
#include <cpprest/http_listener.h>              // HTTP server
#include <cpprest/json.h>                       // JSON library
#include <cpprest/uri.h>                        // URI library
#include <cpprest/http_msg.h>

// from xsapi\types.h

#include <string>
#include <regex>
#include <chrono>

#endif //_WIN32

#ifndef _WIN32
#ifdef _In_
#undef _In_
#endif
#define _In_
#ifdef _Ret_maybenull_
#undef _Ret_maybenull_
#endif
#define _Ret_maybenull_
#ifdef _Post_writable_byte_size_
#undef _Post_writable_byte_size_
#endif
#define _Post_writable_byte_size_(X)
#endif

#ifndef _T
#ifdef _WIN32
#define _T(x) L ## x
#else
#define _T(x) x
#endif
#endif

#if defined _WIN32
#ifdef _NO_MIXERIMP
#define _MIXERIMP
#define _MIXERIMP_DEPRECATED __declspec(deprecated)
#else
#ifdef _MIXERIMP_EXPORT
#define _MIXERIMP __declspec(dllexport)
#define _MIXERIMP_DEPRECATED __declspec(dllexport, deprecated)
#else
#define _MIXERIMP __declspec(dllimport)
#define _MIXERIMP_DEPRECATED __declspec(dllimport, deprecated)
#endif
#endif
#else
#if defined _NO_MIXERIMP || __GNUC__ < 4
#define _MIXERIMP
#define _MIXERIMP_DEPRECATED __attribute__ ((deprecated))
#else
#define _MIXERIMP __attribute__ ((visibility ("default")))
#define _MIXERIMP_DEPRECATED __attribute__ ((visibility ("default"), deprecated))
#endif
#endif

#if !defined(_WIN32) | (defined(_MSC_VER) && (_MSC_VER >= 1900))
// VS2013 doesn't support default move constructor and assignment, so we implemented this.
// However, a user defined move constructor and assignment will implicitly delete default copy 
// constructor and assignment in other compiler like clang. So we only define this in Win32 under VS2013
#define DEFAULT_MOVE_ENABLED
#endif

typedef int32_t function_context;
#ifdef _WIN32
typedef wchar_t char_t;
typedef std::wstring string_t;
typedef std::wstringstream stringstream_t;
//typedef std::wregex regex_t;
//typedef std::wsmatch smatch_t;
#else
typedef char char_t;
typedef std::string string_t;
typedef std::stringstream stringstream_t;
typedef std::regex regex_t;
typedef std::smatch smatch_t;
#endif

#if _MSC_VER <= 1800
typedef std::chrono::system_clock chrono_clock_t;
#else
typedef std::chrono::steady_clock chrono_clock_t;
#endif

// for streamlined build
#include <sstream>
#include "namespaces.h"