#include "ltr_xsl.h"

typedef struct group_entry_ {
    XMLSTRING *actions;
    int action_count;
} group_entry;

typedef struct library_entry_ {
    XMLDICT *groups;
} library_entry;

void XSLTDefineGroupRights(XSLTGLOBALDATA *data, char *library, char *group, char **actions, int action_count)
{
    memory_allocator_set_current(data->allocator);

    XMLSTRING library_string = xmls_new_string_literal(library);
    library_entry *rights = (library_entry *) dict_find(data->group_rights, library_string);
    if (rights == NULL)
    {
        rights = memory_allocator_new(sizeof(library_entry));
        rights->groups = dict_new(50);
        dict_add(data->group_rights, library_string, rights);
    }
    
    group_entry *entry = memory_allocator_new(sizeof(group_entry));
    entry->actions = memory_allocator_new(action_count * sizeof(XMLSTRING));
    entry->action_count = action_count;
    for(int i = 0; i < action_count; i++)
    {
        entry->actions[i] = xmls_new_string_literal(actions[i]); 
    }
    
    dict_add(rights->groups, xmls_new_string_literal(group), entry);
}

void XSLTSetUserContext(TRANSFORM_CONTEXT *ctx, char *library, char **groups, int group_count)
{
    memory_allocator_set_current(ctx->allocator);

    dict_free(ctx->user_rights);
    ctx->user_rights = dict_new(50);

    XMLSTRING library_string = xmls_new_string_literal(library);
    library_entry *rights = (library_entry *) dict_find(ctx->gctx->group_rights, library_string);
    if (rights == NULL)
    {
        error("XSLTSetUserContext:: unknown library: %s", library);
        return;
    }

    for (int i = 0; i < group_count; i++)
    {
        XMLSTRING group_string = xmls_new_string_literal(groups[i]);
        group_entry *entry = (group_entry *) dict_find(rights->groups, group_string);
        if (entry == NULL)
        {
            error("XSLTSetUserContext:: unknown group: %s", group_string->s);
            continue;
        }

        for (size_t j = 0; j < entry->action_count; j++)
        {
            XMLSTRING action = entry->actions[j];
            dict_add(ctx->user_rights, action, action);
        }
    }
}
