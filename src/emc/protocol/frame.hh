// Framing and stream setup for the client protocol prototype.
//
// A frame is a 32-bit big-endian length followed by one flatbuffer Envelope.
// Nothing in this file knows what the buffer means, which is what keeps the
// transport replaceable: unix socket and TCP are the same stream, and a
// WebSocket adapter is the same stream again with its own framing underneath.
#ifndef LINUXCNC_PROTOCOL_FRAME_HH
#define LINUXCNC_PROTOCOL_FRAME_HH

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace linuxcnc {
namespace protocol {

constexpr uint32_t kMaxFrame = 1u << 20;	// 1 MiB, plenty for one Status

inline bool write_all(int fd, const void *data, size_t len) {
    const uint8_t *p = static_cast<const uint8_t *>(data);
    while (len > 0) {
	const ssize_t n = ::send(fd, p, len, MSG_NOSIGNAL);
	if (n > 0) {
	    p += n;
	    len -= static_cast<size_t>(n);
	    continue;
	}
	if (n < 0 && errno == EINTR) {
	    continue;
	}
	return false;
    }
    return true;
}

inline bool frame_send(int fd, const uint8_t *data, size_t len) {
    if (len == 0 || len > kMaxFrame) {
	return false;
    }
    const uint8_t header[4] = {
	static_cast<uint8_t>(len >> 24), static_cast<uint8_t>(len >> 16),
	static_cast<uint8_t>(len >> 8), static_cast<uint8_t>(len)};
    return write_all(fd, header, sizeof(header)) && write_all(fd, data, len);
}

/* 1 = one frame in out, 0 = nothing complete yet, -1 = closed or broken. */
inline int frame_recv(int fd, std::vector<uint8_t> &pending,
		      std::vector<uint8_t> &out) {
    uint8_t buf[4096];
    for (;;) {
	const ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
	if (n > 0) {
	    pending.insert(pending.end(), buf, buf + n);
	} else if (n == 0) {
	    return -1;
	} else if (errno == EINTR) {
	    continue;
	} else if (errno == EAGAIN || errno == EWOULDBLOCK) {
	    break;
	} else {
	    return -1;
	}
	if (pending.size() >= 4) {
	    const uint32_t len = (static_cast<uint32_t>(pending[0]) << 24) |
				 (static_cast<uint32_t>(pending[1]) << 16) |
				 (static_cast<uint32_t>(pending[2]) << 8) |
				 static_cast<uint32_t>(pending[3]);
	    if (len == 0 || len > kMaxFrame) {
		return -1;
	    }
	    if (pending.size() >= 4 + len) {
		out.assign(pending.begin() + 4, pending.begin() + 4 + len);
		pending.erase(pending.begin(), pending.begin() + 4 + len);
		return 1;
	    }
	}
    }
    return 0;
}

inline bool set_nonblocking(int fd) {
    const int flags = ::fcntl(fd, F_GETFL, 0);
    return flags >= 0 && ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

/* A listening socket on path, or on the loopback port.  Both are the same
   kind of byte stream once a client is accepted. */
inline int listen_unix(const std::string &path) {
    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    if (path.size() >= sizeof(addr.sun_path)) {
	return -1;
    }
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    ::unlink(path.c_str());
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
	return -1;
    }
    if (::bind(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) !=
	    0 ||
	::listen(fd, 8) != 0) {
	::close(fd);
	return -1;
    }
    return fd;
}

inline int listen_tcp(int port) {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
	return -1;
    }
    int one = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (::bind(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) !=
	    0 ||
	::listen(fd, 8) != 0) {
	::close(fd);
	return -1;
    }
    return fd;
}

inline int connect_unix(const std::string &path) {
    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    if (path.size() >= sizeof(addr.sun_path)) {
	return -1;
    }
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
	return -1;
    }
    if (::connect(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) !=
	0) {
	::close(fd);
	return -1;
    }
    return fd;
}

inline int connect_tcp(int port) {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
	return -1;
    }
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (::connect(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) !=
	0) {
	::close(fd);
	return -1;
    }
    return fd;
}

}  // namespace protocol
}  // namespace linuxcnc

#endif
