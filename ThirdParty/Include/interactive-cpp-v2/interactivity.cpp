#include "internal/common.cpp"
#include "internal/http_client.cpp"
#include "internal/interactive_auth.cpp"
#include "internal/interactive_control.cpp"
#include "internal/interactive_group.cpp"
#include "internal/interactive_participant.cpp"
#include "internal/interactive_scene.cpp"
#include "internal/interactive_session.cpp"
#include "internal/interactive_session_internal.cpp"
#if _DURANGO || defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PC_APP
#include "internal/winapp_http_client.cpp"
#include "internal/winapp_websocket.cpp"
#elif _WIN32
#include "internal/win_http_client.cpp"
#include "internal/win_websocket.cpp"
#endif