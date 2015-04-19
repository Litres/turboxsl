#ifndef INSTRUCTIONS_H_
#define INSTRUCTIONS_H_

#include "ltr_xsl.h"

void instructions_create();

void instructions_release();

int instructions_is_xsl(XMLNODE *instruction);

void instructions_process(template_context *context, XMLNODE *instruction);

#endif
