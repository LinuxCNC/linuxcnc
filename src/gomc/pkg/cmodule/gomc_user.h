/*
 * Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
 * License: LGPL Version 2.1
 */
// gomc_user.h — Userspace module helpers for gomc C modules.
//
// Provides an eventfd-based exit notification mechanism that integrates
// cleanly with select()/poll()/epoll() main loops.
//
// Usage in a simple polling loop:
//   void user_mainloop(void) {
//       while (!GOMC_SHOULD_EXIT()) {
//           // ... do work ...
//           usleep(100000);
//       }
//   }
//
// Usage in a select()-based loop:
//   void user_mainloop(void) {
//       int efd = GOMC_EXIT_FD();
//       int devfd = open("/dev/mydevice", O_RDONLY);
//       while (1) {
//           fd_set rfds;
//           FD_ZERO(&rfds);
//           FD_SET(efd, &rfds);
//           FD_SET(devfd, &rfds);
//           int nfds = (efd > devfd ? efd : devfd) + 1;
//           if (select(nfds, &rfds, NULL, NULL, NULL) < 0) break;
//           if (FD_ISSET(efd, &rfds)) break;  // exit requested
//           if (FD_ISSET(devfd, &rfds)) { /* handle device data */ }
//       }
//       close(devfd);
//   }

#ifndef GOMC_USER_H
#define GOMC_USER_H

#include <sys/eventfd.h>
#include <unistd.h>
#include <poll.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// gomc_should_exit — non-blocking check whether the exit eventfd is signalled.
// Returns non-zero if the module should exit.
static inline int gomc_should_exit(int exit_fd) __attribute__((unused));
static inline int gomc_should_exit(int exit_fd) {
    struct pollfd pfd = { .fd = exit_fd, .events = POLLIN };
    return poll(&pfd, 1, 0) > 0;
}

// gomc_signal_exit — signal the exit eventfd (called by generated Stop()).
static inline void gomc_signal_exit(int exit_fd) __attribute__((unused));
static inline void gomc_signal_exit(int exit_fd) {
    uint64_t val = 1;
    (void)write(exit_fd, &val, sizeof(val));
}

#ifdef __cplusplus
}
#endif

#endif // GOMC_USER_H
