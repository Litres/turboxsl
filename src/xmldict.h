/*
 *  Dictionary for hashed names -> pointers conversion
 *
 */

typedef struct _dict_entry {
  char *name;
  void *data;
  unsigned next;
} XMLDICTENTRY;

typedef struct _dict {
  XMLDICTENTRY *entries;
  unsigned allocated;
  unsigned used;
  unsigned hash[128];
} XMLDICT;

XMLDICT *dict_new(unsigned size);
void dict_free(XMLDICT *dict);
void dict_free_data(XMLDICT *dict);
void *dict_find(XMLDICT *dict, char *name);
int dict_add(XMLDICT *dict, char *name, void *data);
void dict_replace(XMLDICT *dict, char *name, void *data);
