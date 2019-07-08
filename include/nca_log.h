#pragma once

#include <syslog.h>

#define NCA_LOG_INIT(entity)                                                   \
	do {                                                                   \
		openlog(entity, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);  \
	} while (0)

#define NCA_LOG_CLOSE()                                                        \
	do {                                                                   \
		closelog();                                                    \
	} while (0)

/* debug-level message */
#define NCA_LOG_DEBUG(f_, ...)                                                 \
	do {                                                                   \
		syslog(LOG_DEBUG, f_, ##__VA_ARGS__);                          \
	} while (0)

/* informational message */
#define NCA_LOG_INFO(f_, ...)                                                  \
	do {                                                                   \
		syslog(LOG_INFO, f_, ##__VA_ARGS__);                           \
	} while (0);

/* normal, but significant, condition */
#define NCA_LOG_NOICE(f_, ...)                                                 \
	do {                                                                   \
		syslog(LOG_NOTICE, f_, ##__VA_ARGS__);                         \
	} while (0)

/* warning conditions */
#define NCA_LOG_WARN(f_, ...)                                                  \
	do {                                                                   \
		syslog(LOG_WARNING, f_, ##__VA_ARGS__);                        \
	} while (0)

/* error conditions */
#define NCA_LOG_ERROR(f_, ...)                                                 \
	do {                                                                   \
		syslog(LOG_ERR, f_, ##__VA_ARGS__);                            \
	} while (0)

/* critical conditions */
#define NCA_LOG_CRIT(f_, ...)                                                  \
	do {                                                                   \
		syslog(LOG_CRIT, f_, ##__VA_ARGS__);                           \
	} while (0)

/* action must be taken immediately */
#define NCA_LOG_ALERT(f_, ...)                                                 \
	do {                                                                   \
		syslog(LOG_ALERT, f_, ##__VA_ARGS__);                          \
	} while (0)

/* system is unusable */
#define NCA_LOG_EMERG(f_, ...)                                                 \
	do {                                                                   \
		syslog(LOG_EMERG, f_, ##__VA_ARGS__);                          \
	} while (0)
