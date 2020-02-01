#pragma once

#include <curl/curl.h>
#include <event2/event.h>
#include <event2/event_struct.h>
#include <thread>

class CURLHelper {
    private:
        /* Information associated with a specific cURL easy handle */ 
        typedef struct _ConnectionInfo
        {
            CURL* handle;
            char* url;
            std::shared_ptr<std::string> data;
            std::function<void(const std::string& data)> completionCallback;
            curl_off_t total;
            curl_off_t xferred;
            char error[CURL_ERROR_SIZE];
            CURLHelper* owner;
        } ConnectionInfo;
        
        /* Information associated with a specific socket */ 
        typedef struct _SocketInfo
        {
            curl_socket_t sockfd;
            CURL *handle;
            int action;
            long timeout;
            struct event ev;
            CURLHelper* owner;
        } SocketInfo;

        CURLHelper();
        ~CURLHelper();

        void threadBegin();

        std::thread cURLThread;

        // Sockets
        static void removeSocket(SocketInfo* socket);
        static void setSocket(SocketInfo* socket, curl_socket_t s, CURL *easy, int act, void* userData);
        static void addSocket(curl_socket_t s, CURL *easy, int action, void* userData);

        // cURL multi
        ConnectionInfo* newConnection(const char *url, std::function<void(const std::string& data)> completionCallback);
        static int multiTimerCallback(CURLM *multi, long timeout_ms, void* userData);
        static void checkMultiInfo(CURLHelper* helperInstance);
        static int multiSocketCallback(CURL *easy, curl_socket_t s, int what, void* userData, void* socketData);

        // cURL handle options
        static size_t writeCallback(char* data, size_t size, size_t nmemb, void* userData);
        static int progressCallback(void* userData, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);

        // libevent2
        static void eventCallback(int fd, short kind, void* userData);
        static void timerCallback(int fd, short kind, void* userData);
        
        CURLM* multiHandle;
        int currentTransfers;
        bool stopped;

        // libevent2 structures
        struct event_base* eventBase;
        struct event timerEvent;

    public:
        typedef struct _Connection {
            public:
                const char* url;
                std::shared_ptr<std::string> data;
        } Connection;

        static CURLHelper& get();

        Connection addRequest(const std::string& url, std::function<void(const std::string& data)> completionCallback);
        Connection addRequest(const std::string& url);
        void blockForAllTransfers();
};