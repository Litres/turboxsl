#include "xmldict.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
    const char *key = "key";
    const char *value = "value";

    XMLDICT *dictionary = dict_new(10);
    if (dict_find(dictionary, key) != NULL)
    {
        printf("ERROR: key already exists");
        return 0;
    }

    if (!dict_add(dictionary, key, value))
    {
        printf("ERROR: failed to add value");
        return 0;
    }

    const char *result = dict_find(dictionary, key);
    if (result == NULL)
    {
        printf("ERROR: key not found");
        return 0;
    }

    if (strcmp(result, value) != 0)
    {
        printf("ERROR: wrong value");
        return 0;
    }

    return 0;
}