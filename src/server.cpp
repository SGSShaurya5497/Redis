#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <poll.h>
#include <vector>

// Maximum message size we'll accept
const size_t k_max_msg = 4096;
enum {
    STATE_REQ = 0,
    STATE_RES = 1,
    STATE_END = 2,
};

struct Conn {
    int fd = -1;

    uint32_t state = STATE_REQ;

    size_t rbuf_size = 0;
    uint8_t rbuf[4 + k_max_msg];

    size_t wbuf_size = 0;
    size_t wbuf_sent = 0;
    uint8_t wbuf[4 + k_max_msg];
};

static std::vector<Conn *> fd2conn;


static void fd_set_nb(int fd) {
    errno = 0;
    int flags = fcntl(fd, F_GETFL, 0);
    if(errno) {
        perror("fcntl");
        return;
    }

    flags |= O_NONBLOCK;
    errno = 0;
    fcntl(fd, F_SETFL, flags);
    if(errno) {
        perror("fcntl");
    }
}

// ------------------------------------------------------------
// Reads EXACTLY n bytes from the socket.
// read() might return fewer bytes than requested,
// so we keep calling read() until we've got everything.
// ------------------------------------------------------------
static int32_t read_full(int fd, char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = read(fd, buf, n);

        // Client disconnected or error occurred
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
// Writes EXACTLY n bytes.
// write() may not send everything in one call,
// so we keep writing until all bytes are sent.
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
// Handles ONE request from the client.
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
static int32_t one_request(int connfd) {

    // Buffer:
    // 4 bytes for length
    // 4096 bytes for message
    // 1 extra byte for '\0'
    char rbuf[4 + k_max_msg + 1];

    errno = 0;

    // -----------------------------
    // STEP 1:
    // Read message length
    // -----------------------------
    int32_t err = read_full(connfd, rbuf, 4);

    if (err) {
        if (errno == 0)
            printf("EOF\n");
        else
            printf("read error\n");

        return err;
    }

    // Convert first 4 bytes into integer length
    uint32_t len = 0;
    memcpy(&len, rbuf, 4);

    if (len > k_max_msg) {
        printf("Message too long\n");
        return -1;
    }

    // -----------------------------
    // STEP 2:
    // Read actual message
    // -----------------------------
    err = read_full(connfd, &rbuf[4], len);

    if (err) {
        printf("read error\n");
        return err;
    }

    // Make it printable
    rbuf[4 + len] = '\0';

    printf("Client says: %s\n", &rbuf[4]);

    // -----------------------------
    // STEP 3:
    // Prepare reply
    // -----------------------------
    const char reply[] = "world";

    char wbuf[4 + sizeof(reply)];

    len = (uint32_t)strlen(reply);

    // Put reply length into first 4 bytes
    memcpy(wbuf, &len, 4);

    // Put actual reply after header
    memcpy(&wbuf[4], reply, len);

    // Send reply
    return write_all(connfd, wbuf, 4 + len);
}
static void connection_io(Conn *conn) {

}
static void accept_new_conn(int fd) {

    sockaddr_in client_addr{};
    socklen_t socklen = sizeof(client_addr);

    int connfd = accept(fd, (sockaddr *)&client_addr, &socklen);

    if (connfd < 0) {
        return;
    }

    // Make the client socket nonblocking too
    fd_set_nb(connfd);

    // Create a new connection object
    Conn *conn = new Conn();

    conn->fd = connfd;

    // Make sure vector is large enough
    if (fd2conn.size() <= (size_t)connfd) {
        fd2conn.resize(connfd + 1);
    }

    // Store connection using fd as index
    fd2conn[connfd] = conn;

    printf("New client connected! fd = %d\n", connfd);
}


int main() {

    // ----------------------------------------
    // Create TCP socket
    // ----------------------------------------
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd < 0) {
        perror("socket");
        return 1;
    }

    // Allows restarting server immediately
    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    // ----------------------------------------
    // Bind socket to port 1234
    // ----------------------------------------
    sockaddr_in addr{};

    addr.sin_family = AF_INET;

    addr.sin_port = htons(1234);

    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd, (sockaddr*)&addr, sizeof(addr))) {
        perror("bind");
        return 1;
    }

    // ----------------------------------------
    // Start listening
    // ----------------------------------------
    if (listen(fd, SOMAXCONN)) {
        perror("listen");
        return 1;
    }
    fd_set_nb(fd);

    printf("Server listening on port 1234...\n");

    // Server runs forever
    while (true) {

    std::vector<pollfd> poll_args;

    // Listening socket
    pollfd pfd{};
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    poll_args.push_back(pfd);

    // Existing client connections
    for (Conn *conn : fd2conn) {

        if (!conn)
            continue;

        pollfd pfd{};
        pfd.fd = conn->fd;
        pfd.events = POLLIN;
        pfd.revents = 0;

        poll_args.push_back(pfd);
    }

    int rv = poll(poll_args.data(),
                  poll_args.size(),
                  -1);

    if (rv < 0) {
        perror("poll");
        continue;
    }

    printf("Something became ready!\n");
}


    close(fd);

    return 0;
}
