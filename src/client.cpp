#include <arpa/inet.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdint>

// Maximum message size
const size_t k_max_msg = 4096;

// ------------------------------------------------------------
// Reads exactly n bytes from the socket.
// ------------------------------------------------------------
static int32_t read_full(int fd, char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = read(fd, buf, n);

        if (rv <= 0) {
            return -1;
        }

        assert((size_t)rv <= n);

        n -= (size_t)rv;
        buf += rv;
    }

    return 0;
}

// ------------------------------------------------------------
// Writes exactly n bytes to the socket.
// ------------------------------------------------------------
static int32_t write_all(int fd, const char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = write(fd, buf, n);

        if (rv <= 0) {
            return -1;
        }

        assert((size_t)rv <= n);

        n -= (size_t)rv;
        buf += rv;
    }

    return 0;
}

// ------------------------------------------------------------
// Sends ONE request to the server
// and receives ONE reply.
//
// Protocol:
//
// +----------+-----------+
// | 4 bytes  | Message   |
// | Length   | Body      |
// +----------+-----------+
//
// Example:
//
// [6][hello1]
// ------------------------------------------------------------
static int32_t query(int fd, const char *text) {

    uint32_t len = (uint32_t)strlen(text);

    if (len > k_max_msg)
        return -1;

    // Buffer that will contain:
    // [length][message]
    char wbuf[4 + k_max_msg];

    // First 4 bytes = message length
    memcpy(wbuf, &len, 4);

    // Remaining bytes = actual message
    memcpy(&wbuf[4], text, len);

    // Send complete packet
    if (write_all(fd, wbuf, 4 + len))
        return -1;

    //--------------------------------------------------
    // Receive reply header
    //--------------------------------------------------

    char rbuf[4 + k_max_msg + 1];

    errno = 0;

    if (read_full(fd, rbuf, 4)) {

        if (errno == 0)
            printf("EOF\n");
        else
            printf("Read error\n");

        return -1;
    }

    // Extract reply length
    memcpy(&len, rbuf, 4);

    if (len > k_max_msg) {
        printf("Reply too long\n");
        return -1;
    }

    //--------------------------------------------------
    // Receive reply body
    //--------------------------------------------------

    if (read_full(fd, &rbuf[4], len)) {
        printf("Read error\n");
        return -1;
    }

    // Convert bytes into C-string
    rbuf[4 + len] = '\0';

    printf("Server says: %s\n", &rbuf[4]);

    return 0;
}

int main() {

    //--------------------------------------------------
    // Create TCP socket
    //--------------------------------------------------
        
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd < 0) {
        perror("socket");
        return 1;
    }

    //--------------------------------------------------
    // Address of the server
    //--------------------------------------------------

    sockaddr_in addr{};

    addr.sin_family = AF_INET;

    addr.sin_port = htons(1234);

    // 127.0.0.1 = localhost
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    //--------------------------------------------------
    // Connect to server
    //--------------------------------------------------

    if (connect(fd, (sockaddr *)&addr, sizeof(addr))) {
        perror("connect");
        return 1;
    }

    printf("Connected to server!\n\n");

    //--------------------------------------------------
    // Send multiple requests
    //--------------------------------------------------

    query(fd, "hello1");

    query(fd, "hello2");

    query(fd, "hello3");

    //--------------------------------------------------
    // Close connection
    //--------------------------------------------------

    close(fd);

    return 0;
}
