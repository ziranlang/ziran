#!/bin/sh
set -eu

tool_dir=$(dirname "$1")
work=$(mktemp -d)
server_pid=""
cleanup() {
    if [ -n "$server_pid" ]; then kill "$server_pid" 2>/dev/null || true; fi
    rm -rf "$work"
}
trap cleanup EXIT HUP INT TERM

cat > "$work/server.c" <<'EOF'
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int send_all(int fd, const unsigned char *data, size_t length) {
    size_t offset = 0;
    while(offset < length) {
        ssize_t count = send(fd, data + offset, length - offset, MSG_NOSIGNAL);
        if(count < 0 && errno == EINTR) continue;
        if(count <= 0) return 0;
        offset += (size_t)count;
    }
    return 1;
}

int main(void) {
    signal(SIGPIPE, SIG_IGN);
    alarm(5);
    int listener = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if(listener < 0 || bind(listener, (struct sockaddr *)&address, sizeof(address)) != 0 ||
       listen(listener, 1) != 0) return 10;
    socklen_t size = sizeof(address);
    if(getsockname(listener, (struct sockaddr *)&address, &size) != 0) return 11;
    printf("%u\n", (unsigned)ntohs(address.sin_port));
    if(fflush(stdout) != 0) return 12;
    int client = accept(listener, NULL, NULL);
    close(listener);
    if(client < 0) return 13;
    unsigned char request[4];
    size_t offset = 0;
    while(offset < sizeof(request)) {
        ssize_t count = recv(client, request + offset, sizeof(request) - offset, 0);
        if(count < 0 && errno == EINTR) continue;
        if(count <= 0) return 14;
        offset += (size_t)count;
    }
    static const unsigned char expected[4] = {'D', 'E', 'X', '!'};
    static const unsigned char response[7] = {'I', 'Z', 'I', 'R', 'A', 'N', '!'};
    if(memcmp(request, expected, sizeof(expected)) != 0 || !send_all(client, response, sizeof(response))) return 15;
    return close(client) == 0 ? 0 : 16;
}
EOF
"${CC:-cc}" -std=c11 -D_GNU_SOURCE "$work/server.c" -o "$work/server"
"$work/server" > "$work/port" &
server_pid=$!
attempt=0
while [ ! -s "$work/port" ]; do
    attempt=$((attempt + 1))
    if [ "$attempt" -gt 100 ] || ! kill -0 "$server_pid" 2>/dev/null; then
        echo 'loopback test server did not start' >&2
        exit 1
    fi
    sleep 0.01
done
port=$(tail -n 1 "$work/port")

"$1" check --root tests/spec --module-path std tests/spec/socket_linux_test.zi
"$tool_dir/zi2c" --no-main --root tests/spec --module-path std \
    -o "$work/c" tests/spec/socket_linux_test.zi
cat > "$work/c/main.c" <<EOF
#include "socket_linux_test.h"
#include <stdint.h>
int main(void) { return SelfTest((uint16_t)$port) == 42 ? 0 : 1; }
EOF
"${CC:-cc}" -std=c11 -Iinclude -I"$work/c" \
    "$work/c/socket_linux.c" "$work/c/socket_linux_test.c" "$work/c/main.c" \
    -o "$work/client"
env -u DISPLAY -u WAYLAND_DISPLAY "$work/client"
wait "$server_pid"
server_pid=""
echo "Loopback TCP socket native C test passed"
