#include "localization.h"

#include <gettext-po.h>

#include "logger.h"
#include "allocator.h"
#include "xmldict.h"
#include "ltr_xsl.h"

struct localization_ {
    XMLDICT *cache;
    struct localization_entry_ *root_entry;
};

struct localization_entry_ {
    po_file_t file;
    int (*get_plural)(int);
    XMLDICT *table;
    struct localization_entry_ *next_entry;
};

void po_xerror(int severity, po_message_t message, const char *filename, size_t lineno, size_t column, int multiline_p, const char *message_text)
{

}

void po_xerror2(int severity, po_message_t message1, const char *filename1, size_t lineno1, size_t column1, int multiline_p1,
    const char *message_text1, po_message_t message2, const char *filename2, size_t lineno2, size_t column2, int multiline_p2,
    const char *message_text2)
{

}

static const struct po_xerror_handler po_handler = {po_xerror, po_xerror2};

int localization_get_plural_ru(int n)
{
    return n % 10 == 1 && n % 100 != 11 ? 0 : n % 10 >= 2 && n % 10 <= 4 && (n % 100 < 10 || n % 100 >= 20) ? 1 : 2;
}

int localization_get_plural_uk(int n)
{
    return n % 10 == 1 && n % 100 != 11 ? 0 : n % 10 >= 2 && n % 10 <= 4 && (n % 100 < 10 || n % 100 >= 20) ? 1 : 2;
}

int localization_get_plural_en(int n)
{
    return n != 1;
}

int localization_get_plural_pl(int n)
{
    return n == 1 ? 0 : n % 10 >= 2 && n % 10 <= 4 && (n % 100 < 10 || n % 100 >= 20) ? 1 : 2;
}

int localization_get_plural_et(int n)
{
    return n != 1;
}

int localization_get_plural_de(int n)
{
    return n != 1;
}

int localization_get_plural_es(int n)
{
    return n != 1;
}

localization_t *localization_create()
{
    localization_t *result = memory_allocator_new(sizeof(localization_t));

    result->cache = dict_new(32);
    result->root_entry = NULL;

    return result;
}

void localization_release(localization_t *object)
{
    localization_entry_t *current = object->root_entry;
    while (current != NULL)
    {
        localization_entry_t *next = current->next_entry;
        po_file_free(current->file);
        dict_free(current->table);
        current = next;
    }

    dict_free(object->cache);
}

localization_entry_t *localization_entry_create(localization_t *object, const char *filename)
{
    XMLSTRING key = xmls_new_string_literal(filename);

    localization_entry_t *entry = (localization_entry_t *)dict_find(object->cache, key);
    if (entry != NULL)
    {
        return entry;
    }

    entry = memory_allocator_new(sizeof(localization_entry_t));

    entry->file = po_file_read(filename, &po_handler);
    if (entry->file == NULL)
    {
        error("localization_entry_create:: couldn't open the PO file %s", filename);
        return NULL;
    }

    const char *header = po_file_domain_header(entry->file, NULL);
    if (header == NULL)
    {
        error("localization_entry_create:: header not found");
        return NULL;
    }

    const char *field = po_header_field(header, "Language");
    if (field == NULL)
    {
        error("localization_entry_create:: Language field not found");
        return NULL;
    }

    if (strcmp(field, "ru_RU") == 0)
    {
        entry->get_plural = localization_get_plural_ru;
    }
    else if (strcmp(field, "en_US") == 0)
    {
        entry->get_plural = localization_get_plural_en;
    }
    else if (strcmp(field, "et_EE") == 0)
    {
        entry->get_plural = localization_get_plural_et;
    }
    else if (strcmp(field, "pl_PL") == 0)
    {
        entry->get_plural = localization_get_plural_pl;
    }
    else if (strcmp(field, "de_DE") == 0)
    {
        entry->get_plural = localization_get_plural_de;
    }
    else if (strcmp(field, "es_ES") == 0)
    {
        entry->get_plural = localization_get_plural_es;
    }
    else if (strcmp(field, "uk_UA") == 0)
    {
        entry->get_plural = localization_get_plural_uk;
    }
    else
    {
        error("localization_entry_create:: unknown language: %s", field);
        return NULL;
    }

    entry->table = dict_new(32);
    entry->next_entry = NULL;

    po_message_iterator_t iterator = po_message_iterator(entry->file, NULL);
    po_message_t message = po_next_message(iterator);
    while (message != NULL)
    {
        XMLSTRING id = unescape_string(po_message_msgid(message));
        dict_add(entry->table, id, message);
        message = po_next_message(iterator);
    }
    po_message_iterator_free(iterator);

    dict_add(object->cache, key, entry);

    if (object->root_entry == NULL)
    {
        object->root_entry = entry;
    }
    else
    {
        localization_entry_t *t = object->root_entry;
        while (t->next_entry != NULL) t = t->next_entry;
        t->next_entry = entry;
    }

    return entry;
}

const char *localization_entry_get(localization_entry_t *entry, const char *id)
{
    po_message_t message = (po_message_t)dict_find(entry->table, xmls_new_string_literal(id));
    if (message == NULL)
    {
        error("localization_entry_get:: unknown message id: %s", id);
        return NULL;
    }

    return po_message_msgstr(message);
}

const char *localization_entry_get_plural(localization_entry_t *entry, const char *id, int n)
{
    po_message_t message = (po_message_t)dict_find(entry->table, xmls_new_string_literal(id));
    if (message == NULL)
    {
        error("localization_entry_get_plural:: unknown message id: %s", id);
        return NULL;
    }

    int index = entry->get_plural(n);
    return po_message_msgstr_plural(message, index);
}
