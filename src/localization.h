#ifndef LOCALIZATION_H_
#define LOCALIZATION_H_

typedef struct localization_ localization_t;

localization_t *localization_create(const char *filename);
const char *localization_get(localization_t *object, const char *id);
const char *localization_get_plural(localization_t *object, const char *id, int n);
void localization_release(localization_t *cache);

#endif
