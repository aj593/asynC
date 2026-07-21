/*
 * wepoll - epoll for Windows
 * https://github.com/piscisaureus/wepoll
 *
 * Copyright 2012-2020, Bert Belder <bertbelder@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WEPOLL_H_
#define WEPOLL_H_

#ifndef WEPOLL_EXPORT
#define WEPOLL_EXPORT
#endif

#include <stdint.h>

enum EPOLL_EVENTS {
  WEPOLLIN      = (int) (1U <<  0),
  WEPOLLPRI     = (int) (1U <<  1),
  WEPOLLOUT     = (int) (1U <<  2),
  WEPOLLERR     = (int) (1U <<  3),
  WEPOLLHUP     = (int) (1U <<  4),
  WEPOLLRDNORM  = (int) (1U <<  6),
  WEPOLLRDBAND  = (int) (1U <<  7),
  WEPOLLWRNORM  = (int) (1U <<  8),
  WEPOLLWRBAND  = (int) (1U <<  9),
  WEPOLLMSG     = (int) (1U << 10), /* Never reported. */
  WEPOLLRDHUP   = (int) (1U << 13),
  WEPOLLONESHOT = (int) (1U << 31)
};

#define WEPOLLIN      (1U <<  0)
#define WEPOLLPRI     (1U <<  1)
#define WEPOLLOUT     (1U <<  2)
#define WEPOLLERR     (1U <<  3)
#define WEPOLLHUP     (1U <<  4)
#define WEPOLLRDNORM  (1U <<  6)
#define WEPOLLRDBAND  (1U <<  7)
#define WEPOLLWRNORM  (1U <<  8)
#define WEPOLLWRBAND  (1U <<  9)
#define WEPOLLMSG     (1U << 10)
#define WEPOLLRDHUP   (1U << 13)
#define WEPOLLONESHOT (1U << 31)

#define WEPOLL_CTL_ADD 1
#define WEPOLL_CTL_MOD 2
#define WEPOLL_CTL_DEL 3

typedef void* HANDLE;
typedef uintptr_t SOCKET;

typedef union wepoll_data {
  void* ptr;
  int fd;
  uint32_t u32;
  uint64_t u64;
  SOCKET sock; /* Windows specific */
  HANDLE hnd;  /* Windows specific */
} wepoll_data_t;

struct wepoll_event {
  uint32_t events;   /* Epoll events and flags */
  wepoll_data_t data; /* User data variable */
};

#ifdef __cplusplus
extern "C" {
#endif

WEPOLL_EXPORT HANDLE wepoll_create(int size);
WEPOLL_EXPORT HANDLE wepoll_create1(int flags);

WEPOLL_EXPORT int wepoll_close(HANDLE ephnd);

WEPOLL_EXPORT int wepoll_ctl(HANDLE ephnd,
                            int op,
                            SOCKET sock,
                            struct wepoll_event* event);

WEPOLL_EXPORT int wepoll_wait(HANDLE ephnd,
                             struct wepoll_event* events,
                             int maxevents,
                             int timeout);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* WEPOLL_H_ */
