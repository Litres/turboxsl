#ifndef TEMPLATE_TASK_H_
#define TEMPLATE_TASK_H_

#include "ltr_xsl.h"

typedef struct template_task_ template_task;

void template_task_run(XMLNODE *instruction, template_context *context, void (*function)(template_context *));

void template_task_run_and_wait(template_context *context, void (*function)(template_context *));

#endif
