#ifndef TEMPLATE_TASK_H_
#define TEMPLATE_TASK_H_

#include "ltr_xsl.h"

typedef struct template_task_context_ template_task_context;

void template_task_run(template_context *context, void (*function)(template_context *));

void template_task_run_and_wait(template_context *context, void (*function)(template_context *));

#endif
