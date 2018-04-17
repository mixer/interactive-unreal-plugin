#include "interactivity.h"
#include "common.h"
#include "http_client.h"

#include <ctime>
#include <string>
#include "rapidjson\document.h"

#define JSON_GRANTED_AT "granted_at"
#define JSON_REFRESH_TOKEN "refresh_token"
#define JSON_EXPIRES_IN "expires_in"
#define JSON_ACCESS_TOKEN "access_token"
#define JSON_CODE "code"
#define JSON_HANDLE "handle"

namespace mixer_internal
{
	int stamp_token_response(const http_response& response, _Out_ std::string& tokenData)
	{
		// The access token data has an "expires_in" field, mark the token with a local grant time for future use.
		rapidjson::Document doc;
		if (doc.Parse(response.body.c_str(), response.body.length()).HasParseError())
		{
			return MIXER_ERROR_JSON_PARSE;
		}

		doc.AddMember(JSON_GRANTED_AT, std::time(0), doc.GetAllocator());
		tokenData = jsonStringify(doc);
		return MIXER_OK;
	}
}

using namespace mixer_internal;

// Parse the access token out of the refresh token data.
int interactive_auth_parse_refresh_token(const char* refreshToken, char* authorization, size_t* authorizationLength)
{
	if (nullptr == refreshToken || nullptr == authorizationLength)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	rapidjson::Document doc;
	if (doc.Parse(rapidjson::StringRef(refreshToken)).HasParseError())
	{
		return MIXER_ERROR_JSON_PARSE;
	}

	if (!doc.HasMember(JSON_ACCESS_TOKEN))
	{
		return MIXER_ERROR_AUTH_INVALID_TOKEN;
	}

	std::string authorizationStr = std::string("Bearer ") + doc["access_token"].GetString();
	if (nullptr == authorization || *authorizationLength < authorizationStr.length() + 1)
	{
		*authorizationLength = authorizationStr.length() + 1;
		return MIXER_ERROR_BUFFER_SIZE;
	}

	memcpy(authorization, authorizationStr.c_str(), authorizationStr.length());
	authorization[authorizationStr.length()] = 0;
	*authorizationLength = authorizationStr.length() + 1;
	return MIXER_OK;
}

int interactive_auth_get_short_code(const char* clientId, const char* clientSecret, char* shortCode, size_t* shortCodeLength, char* shortCodeHandle, size_t* shortCodeHandleLength)
{
	if (nullptr == clientId || nullptr == shortCode || nullptr == shortCodeLength || nullptr == shortCodeHandle || nullptr == shortCodeHandleLength)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	http_response response;
	std::string oauthCodeUrl = "https://mixer.com/api/v1/oauth/shortcode";

	// Construct the json body
	std::string jsonBody;
	if (nullptr == clientSecret)
	{
		jsonBody = std::string("{ \"client_id\": \"") + clientId + "\", \"scope\": \"interactive:robot:self\" }";
	}
	else
	{
		jsonBody = std::string("{ \"client_id\": \"") + clientId + "\", \"client_secret\": \"" + clientSecret + "\", \"scope\": \"interactive:robot:self\" }";
	}

	std::unique_ptr<http_client> client = http_factory::make_http_client();
	RETURN_IF_FAILED(client->make_request(oauthCodeUrl, "POST", nullptr, jsonBody, response));
	if (200 != response.statusCode)
	{
		return response.statusCode;
	}

	rapidjson::Document doc;
	if (doc.Parse(response.body.c_str()).HasParseError())
	{
		return MIXER_ERROR_JSON_PARSE;
	}

	std::string code = doc[JSON_CODE].GetString();
	std::string handle = doc[JSON_HANDLE].GetString();

	if (*shortCodeLength < code.length() + 1 ||
		*shortCodeHandleLength < handle.length() + 1)
	{
		*shortCodeLength = code.length() + 1;
		*shortCodeHandleLength = handle.length() + 1;
		return MIXER_ERROR_BUFFER_SIZE;
	}

	memcpy(shortCode, code.c_str(), code.length());
	shortCode[code.length()] = 0;
	*shortCodeLength = code.length() + 1;

	memcpy(shortCodeHandle, handle.c_str(), handle.length());
	shortCodeHandle[handle.length()] = 0;
	*shortCodeHandleLength = handle.length() + 1;
	return MIXER_OK;
}

int interactive_auth_wait_short_code(const char* clientId, const char* clientSecret, const char* shortCodeHandle, char* refreshToken, size_t* refreshTokenLength)
{
	// Poll that shortcode until it is validated or times out.
	std::unique_ptr<http_client> httpClient = http_factory::make_http_client();
	http_response response;
	std::string validateUrl = "https://mixer.com/api/v1/oauth/shortcode/check/" + std::string(shortCodeHandle);

	int handleStatus = 204;
	while (204 == handleStatus)
	{
		RETURN_IF_FAILED(httpClient->make_request(validateUrl, "GET", nullptr, "", response));
		handleStatus = response.statusCode;
	}

	std::string oauthCode;
	switch (handleStatus)
	{
	case 200: // OK
	{
		rapidjson::Document doc;
		if (doc.Parse(response.body.c_str()).HasParseError())
		{
			return MIXER_ERROR_JSON_PARSE;
		}

		oauthCode = doc[JSON_CODE].GetString();
		break;
	}
	case 403: // Forbidden - Access denied
	{
		return MIXER_ERROR_AUTH_DENIED;
	}
	case 404: // Not Found - Handle expired
	{
		return MIXER_ERROR_TIMED_OUT;
	}
	default: // Unknown error
	{
		return MIXER_ERROR;
	}
	}

	// Exchange oauth code for oauth token.
	std::string refreshTokenData;
	const std::string exchangeUrl = "https://mixer.com/api/v1/oauth/token";

	std::string jsonBody;
	if (nullptr == clientSecret)
	{
		jsonBody = std::string("{ \"client_id\": \"") + clientId + "\", \"code\": \"" + oauthCode + "\", \"grant_type\": \"authorization_code\" }";
	}
	else
	{
		jsonBody = std::string("{ \"client_id\": \"") + clientId + "\", \"client_secret\": \"" + clientSecret + "\", \"code\": \"" + oauthCode + "\", \"grant_type\": \"authorization_code\" }";
	}

	httpClient->make_request(exchangeUrl, "POST", nullptr, jsonBody, response);
	if (200 != response.statusCode)
	{
		return MIXER_ERROR_AUTH;
	}

	RETURN_IF_FAILED(stamp_token_response(response, refreshTokenData));

	if (*refreshTokenLength < refreshTokenData.length() + 1)
	{
		*refreshTokenLength = refreshTokenData.length() + 1;
		return MIXER_ERROR_BUFFER_SIZE;
	}

	memcpy(refreshToken, refreshTokenData.c_str(), refreshTokenData.length());
	refreshToken[refreshTokenData.length()] = 0;
	*refreshTokenLength = refreshTokenData.length() + 1;

	return MIXER_OK;
}

int interactive_auth_refresh_token(const char* clientId, const char* clientSecret, const char* staleToken, char* refreshToken, size_t* refreshTokenLength)
{
	if (nullptr == clientId || nullptr == staleToken || nullptr == refreshToken || nullptr == refreshTokenLength)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	// Parse the stale token data to get the token string.
	std::string refreshTokenData;
	try
	{
		rapidjson::Document doc;
		if (doc.Parse(staleToken).HasParseError())
		{
			return MIXER_ERROR_JSON_PARSE;
		}

		if (!doc.IsObject() || !doc.HasMember(JSON_REFRESH_TOKEN))
		{
			return MIXER_ERROR_AUTH_INVALID_TOKEN;
		}

		refreshTokenData = doc[JSON_REFRESH_TOKEN].GetString();
	}
	catch (...)
	{
		return MIXER_ERROR_AUTH_INVALID_TOKEN;
	}

	http_response response;
	const std::string exchangeUrl = "https://mixer.com/api/v1/oauth/token";

	std::string jsonBody;
	if (nullptr == clientSecret || 0 == strlen(clientSecret))
	{
		jsonBody = std::string("{ \"client_id\": \"") + clientId + "\", \"refresh_token\": \"" + refreshTokenData + "\", \"grant_type\": \"refresh_token\" }";
	}
	{
		jsonBody = std::string("{ \"client_id\": \"") + clientId + "\", \"client_secret\": \"" + clientSecret + "\", \"refresh_token\": \"" + refreshTokenData + "\", \"grant_type\": \"refresh_token\" }";
	}

	std::unique_ptr<http_client> httpClient = http_factory::make_http_client();
	httpClient->make_request(exchangeUrl, "POST", nullptr, jsonBody, response);
	if (200 != response.statusCode)
	{
		return MIXER_ERROR_AUTH;
	}

	RETURN_IF_FAILED(stamp_token_response(response, refreshTokenData));

	if (*refreshTokenLength < refreshTokenData.length() + 1)
	{
		*refreshTokenLength = refreshTokenData.length() + 1;
		return MIXER_ERROR_BUFFER_SIZE;
	}

	memcpy(refreshToken, refreshTokenData.c_str(), refreshTokenData.length());
	refreshToken[refreshTokenData.length()] = 0;
	*refreshTokenLength = refreshTokenData.length() + 1;

	return MIXER_OK;
}

int interactive_auth_is_token_stale(const char* token, bool* isStale)
{
	if (nullptr == token || nullptr == isStale)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	try
	{
		rapidjson::Document doc;
		if (doc.Parse(token).HasParseError())
		{
			return MIXER_ERROR_JSON_PARSE;
		}

		if (!doc.HasMember(JSON_GRANTED_AT) || !doc.HasMember(JSON_EXPIRES_IN))
		{
			return MIXER_ERROR_AUTH_INVALID_TOKEN;
		}

		int64_t grantedAt = doc[JSON_GRANTED_AT].GetInt64();
		int64_t expiresIn = doc[JSON_EXPIRES_IN].GetInt64();
		*isStale = std::time(nullptr) > grantedAt + (expiresIn / 2);
	}
	catch (std::exception e)
	{
		return MIXER_ERROR_AUTH_INVALID_TOKEN;
	}

	return MIXER_OK;
}