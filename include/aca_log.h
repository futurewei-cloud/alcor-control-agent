// Copyright 2019 The Alcor Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
