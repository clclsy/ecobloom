#include "HttpClient.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <sstream>

int http_post_json(const std::string &host, int port, const std::string &path, const std::string &jsonBody) {
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    std::ostringstream portStr;
    portStr << port;

    if (getaddrinfo(host.c_str(), portStr.str().c_str(), &hints, &result) != 0 || !result) {
        return -1;
    }

    int sockFd = -1;
    struct addrinfo *rp;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sockFd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockFd == -1) {
            continue;
        }
        if (connect(sockFd, rp->ai_addr, rp->ai_addrlen) != -1) {
            break; // connected
        }
        close(sockFd);
        sockFd = -1;
    }
    freeaddrinfo(result);

    if (sockFd == -1) {
        return -2;
    }

    std::ostringstream request;
    request << "POST " << path << " HTTP/1.1\r\n"
            << "Host: " << host << "\r\n"
            << "Content-Type: application/json\r\n"
            << "Content-Length: " << jsonBody.size() << "\r\n"
            << "Connection: close\r\n"
            << "\r\n"
            << jsonBody;

    std::string requestStr = request.str();
    size_t totalSent = 0;
    while (totalSent < requestStr.size()) {
        ssize_t sent = send(sockFd, requestStr.c_str() + totalSent, requestStr.size() - totalSent, 0);
        if (sent <= 0) {
            close(sockFd);
            return -3;
        }
        totalSent += sent;
    }

    // Read just enough of the response to parse the status line.
    char buf[512] = {0};
    ssize_t received = recv(sockFd, buf, sizeof(buf) - 1, 0);
    close(sockFd);

    if (received <= 0) {
        return -4;
    }

    // Response line looks like: "HTTP/1.1 200 OK\r\n..."
    int statusCode = -5;
    if (strncmp(buf, "HTTP/", 5) == 0) {
        const char *spacePos = strchr(buf, ' ');
        if (spacePos) {
            statusCode = atoi(spacePos + 1);
        }
    }
    return statusCode;
}
