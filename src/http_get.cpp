#include "http_get.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>

#include <vector>
#endif

namespace tmf {
namespace {

#ifdef _WIN32
std::wstring utf8_to_wide(const std::string& text) {
    if (text.empty()) {
        return L"";
    }
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) {
        throw UriError("Invalid URL");
    }
    std::wstring wide(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), wide.data(), size);
    return wide;
}

struct InternetHandle {
    HINTERNET value = nullptr;
    explicit InternetHandle(HINTERNET handle = nullptr) : value(handle) {}
    ~InternetHandle() {
        if (value) {
            WinHttpCloseHandle(value);
        }
    }
    InternetHandle(const InternetHandle&) = delete;
    InternetHandle& operator=(const InternetHandle&) = delete;
};
#endif

}  // namespace

HttpGetResult http_get(const std::string& url) {
#ifdef _WIN32
    const std::wstring wide_url = utf8_to_wide(url);
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    std::wstring host(2048, L'\0');
    std::wstring path(8192, L'\0');
    std::wstring extra(8192, L'\0');
    parts.lpszHostName = host.data();
    parts.dwHostNameLength = static_cast<DWORD>(host.size());
    parts.lpszUrlPath = path.data();
    parts.dwUrlPathLength = static_cast<DWORD>(path.size());
    parts.lpszExtraInfo = extra.data();
    parts.dwExtraInfoLength = static_cast<DWORD>(extra.size());
    if (!WinHttpCrackUrl(wide_url.c_str(), 0, 0, &parts)) {
        throw UriError("Invalid URL");
    }
    host.resize(parts.dwHostNameLength);
    path.resize(parts.dwUrlPathLength);
    extra.resize(parts.dwExtraInfoLength);
    const std::wstring object_name = path + extra;
    const bool secure = parts.nScheme == INTERNET_SCHEME_HTTPS;

    InternetHandle session(WinHttpOpen(L"TMF-Forum/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                                       WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session.value) {
        throw IoError("Unable to open HTTP session");
    }
    InternetHandle connection(WinHttpConnect(session.value, host.c_str(), parts.nPort, 0));
    if (!connection.value) {
        throw IoError("Unable to connect");
    }
    InternetHandle request(WinHttpOpenRequest(connection.value, L"GET", object_name.c_str(), nullptr, WINHTTP_NO_REFERER,
                                              WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0));
    if (!request.value) {
        throw IoError("Unable to create request");
    }
    DWORD redirects = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    WinHttpSetOption(request.value, WINHTTP_OPTION_REDIRECT_POLICY, &redirects, sizeof(redirects));
    if (!WinHttpSendRequest(request.value, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.value, nullptr)) {
        throw IoError("Request failed");
    }

    DWORD status = 0;
    DWORD status_size = sizeof(status);
    if (!WinHttpQueryHeaders(request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size, WINHTTP_NO_HEADER_INDEX)) {
        throw IoError("Unable to read status code");
    }

    HttpGetResult result;
    result.status = static_cast<int>(status);
    while (true) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request.value, &available)) {
            throw IoError("Unable to read response");
        }
        if (available == 0) {
            break;
        }
        std::string chunk(available, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(request.value, chunk.data(), available, &read)) {
            throw IoError("Unable to read response");
        }
        result.body.append(chunk.data(), read);
    }
    return result;
#else
    (void)url;
    throw IoError("HTTP fetch is only implemented for Windows");
#endif
}

}  // namespace tmf
