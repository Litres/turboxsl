#ifndef LOGGER_H_
#define LOGGER_H_

#include <stdio.h>
#include <pthread.h>

#include "zlog/src/zlog.h"

extern zlog_category_t *logger_category;

void logger_create();
void logger_release();

#define std_error(M, ...) fprintf(stderr, "ERROR [%p] %d: " M "\n", pthread_self(), __LINE__, ##__VA_ARGS__)

#define info(M, ...) zlog_info(logger_category, M, ##__VA_ARGS__)
#define error(M, ...) zlog_error(logger_category, M, ##__VA_ARGS__)

#ifndef DEBUG
#define debug(M, ...)
#else
#define debug(M, ...) zlog_debug(logger_category, M, ##__VA_ARGS__)
#endif

#endif
