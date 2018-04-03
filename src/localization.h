#ifndef LOCALIZATION_H_
#define LOCALIZATION_H_

typedef struct localization_ localization_t;
typedef struct localization_entry_ localization_entry_t;

localization_t *localization_create();
void localization_release(localization_t *object);

localization_entry_t *localization_entry_create(localization_t *object, const char *filename);
const char *localization_entry_get(localization_entry_t *entry, const char *id);
const char *localization_entry_get_plural(localization_entry_t *entry, const char *id, int n);

#endif
