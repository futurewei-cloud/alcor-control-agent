#pragma once

#include <syslog.h>

#define ACA_LOG_INIT(entity)                                                   \
	do {                                                                   \
		openlog(entity, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);  \
	} while (0)

#define ACA_LOG_CLOSE()                                                        \
	do {                                                                   \
		closelog();                                                    \
	} while (0)

/* debug-level message */
#define ACA_LOG_DEBUG(f_, ...)                                                 \
	do {                                                                   \
		syslog(LOG_DEBUG, f_, ##__VA_ARGS__);                          \
	} while (0)

/* informational message */
#define ACA_LOG_INFO(f_, ...)                                                  \
	do {                                                                   \
		syslog(LOG_INFO, f_, ##__VA_ARGS__);                           \
	} while (0);

/* normal, but significant, condition */
#define ACA_LOG_NOICE(f_, ...)                                                 \
	do {                                                                   \
		syslog(LOG_NOTICE, f_, ##__VA_ARGS__);                         \
	} while (0)

/* warning conditions */
#define ACA_LOG_WARN(f_, ...)                                                  \
	do {                                                                   \
		syslog(LOG_WARNING, f_, ##__VA_ARGS__);                        \
	} while (0)

/* error conditions */
#define ACA_LOG_ERROR(f_, ...)                                                 \
	do {                                                                   \
		syslog(LOG_ERR, f_, ##__VA_ARGS__);                            \
	} while (0)

/* critical conditions */
#define ACA_LOG_CRIT(f_, ...)                                                  \
	do {                                                                   \
		syslog(LOG_CRIT, f_, ##__VA_ARGS__);                           \
	} while (0)

/* action must be taken immediately */
#define ACA_LOG_ALERT(f_, ...)                                                 \
	do {                                                                   \
		syslog(LOG_ALERT, f_, ##__VA_ARGS__);                          \
	} while (0)

/* system is unusable */
#define ACA_LOG_EMERG(f_, ...)                                                 \
	do {                                                                   \
		syslog(LOG_EMERG, f_, ##__VA_ARGS__);                          \
	} while (0)
