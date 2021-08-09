// MIT License
// Copyright(c) 2020 Futurewei Cloud
//
//     Permission is hereby granted,
//     free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction,
//     including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and / or sell copies of the Software, and to permit persons
//     to whom the Software is furnished to do so, subject to the following conditions:
//
//     The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//
//     THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//     FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
//     WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#pragma once
#pragma GCC system_header

#include <syslog.h>

extern bool g_debug_mode;

#define UNUSED(x) (void)(x)
#define QUOTE(...) #__VA_ARGS__

#define ACA_LOG_INIT(entity)                                                   \
  do {                                                                         \
    openlog(entity, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);              \
  } while (0)

#define ACA_LOG_CLOSE()                                                        \
  do {                                                                         \
    closelog();                                                                \
  } while (0)

/* debug-level message */
#define ACA_LOG_DEBUG(f_, ...)                                                 \
  do {                                                                         \
    syslog(LOG_DEBUG, "[%s:%d] " f_, __func__, __LINE__,                     \
		       ##__VA_ARGS__);                                                     \
    if (g_debug_mode) {                                                        \
      fprintf(stdout, f_, ##__VA_ARGS__);                                      \
    }                                                                          \
  } while (0)

/* informational message */
#define ACA_LOG_INFO(f_, ...)                                                  \
  do {                                                                         \
    syslog(LOG_INFO, f_, ##__VA_ARGS__);                                       \
    if (g_debug_mode) {                                                        \
      fprintf(stdout, f_, ##__VA_ARGS__);                                      \
    }                                                                          \
  } while (0)

/* normal, but significant, condition */
#define ACA_LOG_NOTICE(f_, ...)                                                \
  do {                                                                         \
    syslog(LOG_NOTICE, f_, ##__VA_ARGS__);                                     \
    if (g_debug_mode) {                                                        \
      fprintf(stdout, f_, ##__VA_ARGS__);                                      \
    }                                                                          \
  } while (0)

/* warning conditions */
#define ACA_LOG_WARN(f_, ...)                                                  \
  do {                                                                         \
    syslog(LOG_WARNING, f_, ##__VA_ARGS__);                                    \
    if (g_debug_mode) {                                                        \
      fprintf(stdout, f_, ##__VA_ARGS__);                                      \
    }                                                                          \
  } while (0)

/* error conditions */
#define ACA_LOG_ERROR(f_, ...)                                                 \
  do {                                                                         \
    syslog(LOG_ERR, f_, ##__VA_ARGS__);                                        \
    if (g_debug_mode) {                                                        \
      fprintf(stdout, f_, ##__VA_ARGS__);                                      \
    }                                                                          \
  } while (0)

/* critical conditions */
#define ACA_LOG_CRIT(f_, ...)                                                  \
  do {                                                                         \
    syslog(LOG_CRIT, f_, ##__VA_ARGS__);                                       \
    if (g_debug_mode) {                                                        \
      fprintf(stdout, f_, ##__VA_ARGS__);                                      \
    }                                                                          \
  } while (0)

/* action must be taken immediately */
#define ACA_LOG_ALERT(f_, ...)                                                 \
  do {                                                                         \
    syslog(LOG_ALERT, f_, ##__VA_ARGS__);                                      \
    if (g_debug_mode) {                                                        \
      fprintf(stdout, f_, ##__VA_ARGS__);                                      \
    }                                                                          \
  } while (0)

/* system is unusable */
#define ACA_LOG_EMERG(f_, ...)                                                 \
  do {                                                                         \
    syslog(LOG_EMERG, f_, ##__VA_ARGS__);                                      \
    if (g_debug_mode) {                                                        \
      fprintf(stdout, f_, ##__VA_ARGS__);                                      \
    }                                                                          \
  } while (0)
