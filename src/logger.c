#include "logger.h"

zlog_category_t *logger_category = NULL;

void logger_create()
{
    int rc = zlog_init("/etc/apache2/turboxsl.conf");
    if (rc)
    {
        std_error("logger_setup:: init failed");
        return;
    }

    logger_category = zlog_get_category("main");
    if (!logger_category)
    {
        std_error("logger_setup:: get category failed");
        zlog_fini();
        return;
    }
}

void logger_release()
{
    zlog_fini();
}
