/*
 *  Dictionary for hashed names -> pointers conversion
 *
 */
#ifndef XML_DICT_H_
#define XML_DICT_H_

typedef struct _dict XMLDICT;

XMLDICT *dict_new(unsigned size);
void dict_free(XMLDICT *dict);
const void *dict_find(XMLDICT *dict, const char *name);
int dict_add(XMLDICT *dict, const char *name, const void *data);
void dict_replace(XMLDICT *dict, const char *name, const void *data);

#endif
