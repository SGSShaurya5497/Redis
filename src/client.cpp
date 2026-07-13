#include <arpa/inet.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    connect(fd, (sockaddr*)&addr, sizeof(addr));

    char msg[] = "hello";
    write(fd, msg, strlen(msg));

    char buf[64]{};
    ssize_t n = read(fd, buf, sizeof(buf) - 1);

    if (n > 0) {
        printf("server says: %s\n", buf);
    }

    close(fd);
}
