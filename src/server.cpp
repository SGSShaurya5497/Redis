#include <arpa/inet.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    bind(fd, (sockaddr*)&addr, sizeof(addr));
    listen(fd, SOMAXCONN);

    printf("Server listening on port 1234...\n");

    while (true) {
        sockaddr_in client_addr{};
        socklen_t len = sizeof(client_addr);

        int connfd = accept(fd, (sockaddr*)&client_addr, &len);

        char buf[64]{};
        ssize_t n = read(connfd, buf, sizeof(buf) - 1);

        if (n > 0) {
            printf("client says: %s\n", buf);

            char reply[] = "world";
            write(connfd, reply, strlen(reply));
        }

        close(connfd);
    }
}	
