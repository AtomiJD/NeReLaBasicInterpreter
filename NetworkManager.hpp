// NetworkManager.hpp
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif
#ifndef CPPHTTPLIB_OPENSSL_SUPPORT
#define CPPHTTPLIB_OPENSSL_SUPPORT
#endif

#include <string>
#include <vector>
#include <map>
#include <httplib.h> // Include httplib here

// Forward declare NeReLaBasic so we can pass it to methods if needed
class NeReLaBasic;

class NetworkManager {
public:
    NetworkManager(); // Constructor
    ~NetworkManager(); // Destructor to clean up

    //std::unique_ptr<httplib::Client> active_http_client;

    // Stores the HTTP status code of the last request
    int last_http_status_code;

    // Stores custom headers to be sent with subsequent requests
    std::map<std::string, std::string> custom_headers;

    // --- HTTP Client Functions ---
    // Performs an HTTP GET request
    std::string httpGet(const std::string& url);

    // Sets a custom header for subsequent requests
    void setHeader(const std::string& name, const std::string& value);

    // Clears all custom headers
    void clearHeaders();

    // --- TODO: Add HTTP Server functions here later ---
};
