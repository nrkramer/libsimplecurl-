#include "cURLHelper.hpp"

#include <iostream>

#define CURL_HELPER_LOG(format, ...) printf("cURLHelper: " format "\n", ##__VA_ARGS__)

CURLHelper::CURLHelper() {
  eventBase = event_base_new();
  multiHandle = curl_multi_init();
  evtimer_assign(&timerEvent, eventBase, timerCallback, this);

  /* setup the generic multi interface options we want */ 
  curl_multi_setopt(multiHandle, CURLMOPT_SOCKETFUNCTION, &CURLHelper::multiSocketCallback);
  curl_multi_setopt(multiHandle, CURLMOPT_SOCKETDATA, this);
  curl_multi_setopt(multiHandle, CURLMOPT_TIMERFUNCTION, &CURLHelper::multiTimerCallback);
  curl_multi_setopt(multiHandle, CURLMOPT_TIMERDATA, this);
}

CURLHelper::~CURLHelper() {
  if (cURLThread.joinable()) {
    if (eventBase != NULL) {
      event_base_loopexit(eventBase, NULL);
      cURLThread.join();
    }
  }

  event_base_free(eventBase);
  curl_multi_cleanup(multiHandle);
  curl_global_cleanup();
}
 
#define STRING_CASE(code) case code: str = __STRING(code)
static bool checkCURLMCode(const char *where, CURLMcode code)
{
  if(code != CURLM_OK) {
    const char* str;
    switch(code) {
      STRING_CASE(CURLM_BAD_HANDLE); break;
      STRING_CASE(CURLM_BAD_EASY_HANDLE); break;
      STRING_CASE(CURLM_OUT_OF_MEMORY); break;
      STRING_CASE(CURLM_INTERNAL_ERROR); break;
      STRING_CASE(CURLM_UNKNOWN_OPTION); break;
      STRING_CASE(CURLM_LAST); break;
      STRING_CASE(CURLM_BAD_SOCKET); CURL_HELPER_LOG("ERROR: %s returned %s", where, str); return true; break;
      default: str = "CURLM_unknown"; break;
    }
    CURL_HELPER_LOG("ERROR: %s returned %s", where, str);
    return false;
  }

  return true;
}
 
 
/* Update the event timer after curl_multi library calls */ 
int CURLHelper::multiTimerCallback(CURLM *multi, long timeout_ms, void* userData)
{
  CURLHelper* _this = static_cast<CURLHelper*>(userData);
  
  struct timeval timeout;
  (void)multi;
 
  timeout.tv_sec = timeout_ms / 1000;
  timeout.tv_usec = (timeout_ms % 1000) * 1000;
 
  /*
   * if timeout_ms is -1, just delete the timer
   *
   * For all other values of timeout_ms, this should set or *update* the timer
   * to the new value
   */ 
  if(timeout_ms == -1)
    evtimer_del(&_this->timerEvent);
  else /* includes timeout zero */
    evtimer_add(&_this->timerEvent, &timeout);

  return 0;
}
 
 
/* Check for completed transfers, and remove their easy handles */ 
void CURLHelper::checkMultiInfo(CURLHelper* handlerInstance)
{
  char *eff_url;
  CURLMsg *msg;
  int msgs_left;
  ConnectionInfo *conn;
  CURL *easy;
  CURLcode res;
 
  while((msg = curl_multi_info_read(handlerInstance->multiHandle, &msgs_left))) {
    if(msg->msg == CURLMSG_DONE) {
      easy = msg->easy_handle;
      res = msg->data.result;
      curl_easy_getinfo(easy, CURLINFO_PRIVATE, &conn);
      curl_easy_getinfo(easy, CURLINFO_EFFECTIVE_URL, &eff_url);
      CURL_HELPER_LOG("Done: %s %s", eff_url, conn->error);
      if (conn->completionCallback != NULL) {
        conn->completionCallback(*conn->data);
      }
      curl_multi_remove_handle(handlerInstance->multiHandle, easy);
      free(conn->url);
      curl_easy_cleanup(easy);
      free(conn);
    }
  }
}
 
 
 
/* Called by libevent when we get action on a multi socket */ 
void CURLHelper::eventCallback(int fd, short kind, void* userData)
{
  CURLHelper* _this = static_cast<CURLHelper*>(userData);
  if (_this == NULL) {
    CURL_HELPER_LOG("Error getting helper instance during event callback.");
    return;
  }

  CURLMcode returnCode;
 
  int action = ((kind & EV_READ) ? CURL_CSELECT_IN : 0) | ((kind & EV_WRITE) ? CURL_CSELECT_OUT : 0);
 
  returnCode = curl_multi_socket_action(_this->multiHandle, fd, action, &_this->currentTransfers);
  if (!checkCURLMCode("cURLHelper::eventCallback: curl_multi_socket_action", returnCode)) {
    return;
  }
 
  checkMultiInfo(_this);
  if(_this->currentTransfers <= 0) {
    if(evtimer_pending(&_this->timerEvent, NULL)) {
      evtimer_del(&_this->timerEvent);
    }
  }
}

/* Called by libevent when our timeout expires */ 
void CURLHelper::timerCallback(int fd, short kind, void* userData)
{
  (void)fd;
  (void)kind;

  CURLHelper* _this = static_cast<CURLHelper*>(userData);
  if (_this == NULL) {
    CURL_HELPER_LOG("Error getting helper instance during timer callback.");
    return;
  }
  
  CURLMcode returnCode = curl_multi_socket_action(_this->multiHandle, CURL_SOCKET_TIMEOUT, 0, &_this->currentTransfers);
  checkCURLMCode("Timer callback: curl_multi_socket_action", returnCode);
  checkMultiInfo(_this);
}
 
/* Clean up the SockInfo structure */ 
void CURLHelper::removeSocket(SocketInfo* socket)
{
  if(socket) {
    event_del(&socket->ev);
    free(socket);
  }
}

/* Assign information to a SockInfo structure */ 
void CURLHelper::setSocket(SocketInfo* socket, curl_socket_t s, CURL *easy, int act, void* userData)
{
  CURLHelper* _this = static_cast<CURLHelper*>(userData);

  int kind = ((act & CURL_POLL_IN) ? EV_READ : 0) | ((act & CURL_POLL_OUT) ? EV_WRITE : 0) | EV_PERSIST;
 
  socket->sockfd = s;
  socket->action = act;
  socket->handle = easy;
  event_del(&socket->ev);
  event_assign(&socket->ev, _this->eventBase, socket->sockfd, kind, eventCallback, _this);
  event_add(&socket->ev, NULL);
}

/* Initialize a new SockInfo structure */ 
void CURLHelper::addSocket(curl_socket_t s, CURL *easy, int action, void* userData)
{
  CURLHelper* _this = static_cast<CURLHelper*>(userData);
  if (_this == NULL) {
    CURL_HELPER_LOG("Error getting helper instance during add socket callback.");
    return;
  }

  SocketInfo* fdp = (SocketInfo*)calloc(sizeof(SocketInfo), 1);
 
  fdp->owner = _this;
  setSocket(fdp, s, easy, action, _this);
  curl_multi_assign(_this->multiHandle, s, fdp);
}
 
/* CURLMOPT_SOCKETFUNCTION */ 
int CURLHelper::multiSocketCallback(CURL *easy, curl_socket_t s, int what, void* userData, void* socketData)
{
  CURLHelper* _this = static_cast<CURLHelper*>(userData);
  if (_this == NULL) {
    CURL_HELPER_LOG("Error getting helper instance during add socket callback.");
    return -1;
  }

  SocketInfo *socket = (SocketInfo*)socketData;
  const char *whatstr[]={ "none", "IN", "OUT", "INOUT", "REMOVE" };

  if(what == CURL_POLL_REMOVE) {
    removeSocket(socket);
  }
  else {
    if(!socket) {
      addSocket(s, easy, what, _this);
    }
    else {
      setSocket(socket, s, easy, what, _this);
    }
  }

  return 0;
}

/* CURLOPT_WRITEFUNCTION */ 
size_t CURLHelper::writeCallback(char* data, size_t size, size_t nmemb, void* userData)
{
  ConnectionInfo *conn = static_cast<ConnectionInfo*>(userData);
  conn->data->append(data);

  return size * nmemb;
}
 
 
/* CURLOPT_PROGRESSFUNCTION */ 
int CURLHelper::progressCallback(void* userData, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
  ConnectionInfo *conn = static_cast<ConnectionInfo*>(userData);
  (void)ultotal;
  (void)ulnow;

  if ((conn->xferred != dlnow) || (conn->total != dltotal)) {
    conn->total = dltotal;
    conn->xferred = dlnow;

    CURL_HELPER_LOG("Progress: %s (%ld/%ld)", conn->url, dlnow, dltotal);
  }

  return 0;
}
 
/* Create a new easy handle, and add it to the global curl_multi */ 
CURLHelper::ConnectionInfo* CURLHelper::newConnection(const char *url, std::function<void(const std::string& data)> completionCallback)
{
  ConnectionInfo* conn;
  CURLMcode rc;
 
  conn = (ConnectionInfo*)calloc(1, sizeof(ConnectionInfo));
  conn->data = std::make_shared<std::string>();
  conn->completionCallback = completionCallback;
  conn->error[0]='\0';
 
  conn->handle = curl_easy_init();
  if(!conn->handle) {
    CURL_HELPER_LOG("curl_easy_init() failed, not creating connection!");
    return NULL;
  }
  conn->owner = this;
  conn->url = strdup(url);
  curl_easy_setopt(conn->handle, CURLOPT_URL, conn->url);
  curl_easy_setopt(conn->handle, CURLOPT_WRITEFUNCTION, &CURLHelper::writeCallback);
  curl_easy_setopt(conn->handle, CURLOPT_WRITEDATA, conn);
  curl_easy_setopt(conn->handle, CURLOPT_ERRORBUFFER, conn->error);
  curl_easy_setopt(conn->handle, CURLOPT_PRIVATE, conn);
  curl_easy_setopt(conn->handle, CURLOPT_NOPROGRESS, 0L);
  curl_easy_setopt(conn->handle, CURLOPT_XFERINFOFUNCTION, &CURLHelper::progressCallback);
  curl_easy_setopt(conn->handle, CURLOPT_XFERINFODATA, conn);
  curl_easy_setopt(conn->handle, CURLOPT_FOLLOWLOCATION, 1L);
  rc = curl_multi_add_handle(multiHandle, conn->handle);
  checkCURLMCode("newConnection: curl_multi_add_handle", rc);

  return conn;
}

void CURLHelper::threadBegin() {
  // Start event loop
  event_base_dispatch(eventBase);

  CURL_HELPER_LOG("Event loop thread joined the calling thread");
}

CURLHelper::Connection CURLHelper::addRequest(const std::string& url, std::function<void(const std::string& data)> completionCallback) {
  Connection conn;
  ConnectionInfo* cInfo = newConnection(url.c_str(), completionCallback);
  if (cInfo != NULL) {
    conn.url = url.c_str();
    conn.data = cInfo->data;
    if (!cURLThread.joinable()) {
      CURL_HELPER_LOG("Starting event loop thread");
      cURLThread = std::thread(&CURLHelper::threadBegin, this);
    }
  }

  return conn;
}

CURLHelper::Connection CURLHelper::addRequest(const std::string& url) {
  return addRequest(url, NULL);
}

void CURLHelper::blockForAllTransfers() {
  CURL_HELPER_LOG("blockForAllTransfers called, will wait for event thread to exit...");
  cURLThread.join();
}

CURLHelper& CURLHelper::get() {
    static CURLHelper singleton;
    return singleton;
}