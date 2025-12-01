#ifndef NET_H
#define NET_H

int net_listen(int port);
int net_accept(int listen_fd);
int net_connect(const char *host, int port);
int net_send_line(int fd, const char *fmt, ...);
int net_recv_line(int fd, char *buf, int buflen);

#endif //NET_H
