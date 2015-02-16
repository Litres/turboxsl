#ifndef LOGGER_H_
#define LOGGER_H_

#include <stdio.h>
#include <pthread.h>

#define info(M, ...) fprintf(stderr, "INFO [%p] %d: " M "\n", pthread_self(), __LINE__, ##__VA_ARGS__)
#define error(M, ...) fprintf(stderr, "ERROR [%p] %d: " M "\n", pthread_self(), __LINE__, ##__VA_ARGS__)

#ifndef DEBUG
#define debug(M, ...)
#else
#define debug(M, ...) fprintf(stderr, "DEBUG [%p] %d: " M "\n", pthread_self(), __LINE__, ##__VA_ARGS__)
#endif

#endif
