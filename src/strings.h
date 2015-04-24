#ifndef STRINGS_H_
#define STRINGS_H_

#include <string.h>

typedef struct _xmls {
    char *s;
    size_t len;
    size_t allocated;
    unsigned long hash;
} *XMLSTRING;

XMLSTRING xmls_new(size_t bsize);

XMLSTRING xmls_new_string(const char *s, size_t length);

XMLSTRING xmls_new_string_literal(const char *s);

int xmls_equals(XMLSTRING a, XMLSTRING b);

#endif
