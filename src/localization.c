#include "localization.h"

#include <gettext-po.h>

#include "logger.h"
#include "allocator.h"
#include "xmldict.h"

struct localization_ {
    po_file_t file;
    int (*get_plural)(int);
    XMLDICT *table;
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

int localization_get_plural_en(int n)
{
    return n != 1;
}

localization_t *localization_create(const char *filename)
{
	localization_t *result = memory_allocator_new(sizeof(localization_t));

    result->file = po_file_read(filename, &po_handler);
    if (result->file == NULL)
    {
    	error("localization_create:: couldn't open the PO file %s", filename);
    	return NULL;
    }

    const char *header = po_file_domain_header(result->file, NULL);
    if (header == NULL)
    {
        error("localization_create:: header not found");
        return NULL;
    }

    const char *field = po_header_field(header, "Language");
    if (field == NULL)
    {
        error("localization_create:: Language field not found");
        return NULL;
    }

    if (strcmp(field, "ru_RU") == 0)
    {
        result->get_plural = localization_get_plural_ru;
    }
    else if (strcmp(field, "en_US") == 0)
    {
        result->get_plural = localization_get_plural_en;
    }
    else
    {
        error("localization_create:: unknown language: %s", field);
        return NULL;
    }

    result->table = dict_new(32);
    
    po_message_iterator_t iterator = po_message_iterator(result->file, NULL);
    po_message_t message = po_next_message(iterator);
    while (message != NULL)
    {
    	dict_add(result->table, xmls_new_string_literal(po_message_msgid(message)), message);
    	message = po_next_message(iterator);
    }
    po_message_iterator_free(iterator);

    return result;
}

const char *localization_get(localization_t *object, const char *id)
{
	po_message_t message = (po_message_t)dict_find(object->table, xmls_new_string_literal(id));
	if (message == NULL)
	{
        error("localization_get:: unknown message id: %s", id);
		return NULL;
	}

	return po_message_msgstr(message);
}

const char *localization_get_plural(localization_t *object, const char *id, int n)
{
    po_message_t message = (po_message_t)dict_find(object->table, xmls_new_string_literal(id));
    if (message == NULL)
    {
        error("localization_get_plural:: unknown message id: %s", id);
        return NULL;
    }

    int index = object->get_plural(n);
    return po_message_msgstr_plural(message, index);
}

void localization_release(localization_t *object)
{
    if (object == NULL) return;

	po_file_free(object->file);
	dict_free(object->table);
}
