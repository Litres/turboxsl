#ifndef TEMPLATES_H_
#define TEMPLATES_H_

typedef struct template_map_ template_map;

template_map *template_map_create();

void template_map_release(template_map *map);

#endif
