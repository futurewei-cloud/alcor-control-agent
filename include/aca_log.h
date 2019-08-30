#pragma once

#include <syslog.h>
#include "transit_log.h"

extern bool g_debug_mode;

#define ACA_LOG_INIT(entity)  \
	do                        \
	{                         \
		TRN_LOG_INIT(entity); \
	} while (0)

#define ACA_LOG_CLOSE()  \
	do                   \
	{                    \
		TRN_LOG_CLOSE(); \
	} while (0)

/* debug-level message */
#define ACA_LOG_DEBUG(f_, ...)                  \
	do                                          \
	{                                           \
		TRN_LOG_DEBUG(f_, ##__VA_ARGS__);       \
		if (g_debug_mode)                       \
		{                                       \
			fprintf(stdout, f_, ##__VA_ARGS__); \
		}                                       \
	} while (0)

/* informational message */
#define ACA_LOG_INFO(f_, ...)                   \
	do                                          \
	{                                           \
		TRN_LOG_INFO(f_, ##__VA_ARGS__);        \
		if (g_debug_mode)                       \
		{                                       \
			fprintf(stdout, f_, ##__VA_ARGS__); \
		}                                       \
	} while (0)

/* normal, but significant, condition */
#define ACA_LOG_NOTICE(f_, ...)                  \
	do                                          \
	{                                           \
		TRN_LOG_NOTICE(f_, ##__VA_ARGS__);      \
		if (g_debug_mode)                       \
		{                                       \
			fprintf(stdout, f_, ##__VA_ARGS__); \
		}                                       \
	} while (0)

/* warning conditions */
#define ACA_LOG_WARN(f_, ...)                   \
	do                                          \
	{                                           \
		TRN_LOG_WARN(f_, ##__VA_ARGS__);        \
		if (g_debug_mode)                       \
		{                                       \
			fprintf(stdout, f_, ##__VA_ARGS__); \
		}                                       \
	} while (0)

/* error conditions */
#define ACA_LOG_ERROR(f_, ...)                  \
	do                                          \
	{                                           \
		TRN_LOG_ERROR(f_, ##__VA_ARGS__);       \
		if (g_debug_mode)                       \
		{                                       \
			fprintf(stdout, f_, ##__VA_ARGS__); \
		}                                       \
	} while (0)

/* critical conditions */
#define ACA_LOG_CRIT(f_, ...)                   \
	do                                          \
	{                                           \
		TRN_LOG_CRIT(f_, ##__VA_ARGS__);        \
		if (g_debug_mode)                       \
		{                                       \
			fprintf(stdout, f_, ##__VA_ARGS__); \
		}                                       \
	} while (0)

/* action must be taken immediately */
#define ACA_LOG_ALERT(f_, ...)                  \
	do                                          \
	{                                           \
		TRN_LOG_ALERT(f_, ##__VA_ARGS__);       \
		if (g_debug_mode)                       \
		{                                       \
			fprintf(stdout, f_, ##__VA_ARGS__); \
		}                                       \
	} while (0)

/* system is unusable */
#define ACA_LOG_EMERG(f_, ...)                  \
	do                                          \
	{                                           \
		TRN_LOG_EMERG(f_, ##__VA_ARGS__);       \
		if (g_debug_mode)                       \
		{                                       \
			fprintf(stdout, f_, ##__VA_ARGS__); \
		}                                       \
	} while (0)
