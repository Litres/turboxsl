/*
 *  Dictionary for hashed names -> pointers conversion
 *
 */
#ifndef XML_DICT_H_
#define XML_DICT_H_

#include "strings.h"

typedef struct _dict XMLDICT;

XMLDICT *dict_new(unsigned size);
void dict_free(XMLDICT *dict);
const void *dict_find(XMLDICT *dict, XMLSTRING name);
int dict_add(XMLDICT *dict, XMLSTRING name, const void *data);
void dict_replace(XMLDICT *dict, XMLSTRING name, const void *data);

#endif
