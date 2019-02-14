#include "instructions.h"

#include "xsl_elements.h"
#include "template_task.h"

XMLDICT *instructions = NULL;

void instruction_call_template(template_context *context, XMLNODE *instruction)
{
    XMLSTRING template_name = xml_get_attr(instruction, xsl_a_name);
    XMLNODE *template = template_byname(context->context, template_name);
    if (template == NULL)
    {
        error("instruction_call_template:: unknown template: %s, stylesheet: %s", template_name->s,
              context->context->stylesheet->file);
        return;
    }

    // process template parameters
    XMLNODE *parameters = NULL;
    for (XMLNODE *i = instruction->children; i; i = i->next)
    {
        if (!xmls_equals(i->name, xsl_withparam)) continue;

        XMLSTRING name = xml_get_attr(i, xsl_a_name);
        XMLSTRING select = xml_get_attr(i, xsl_a_select);

        XMLNODE *parameter = xml_new_node(context->context, name, ATTRIBUTE_NODE);
        parameter->next = parameters;
        parameters = parameter;

        if (!select)
        {
            parameter->extra.v.nodeset = xml_new_node(context->context, NULL, EMPTY_NODE);
            parameter->extra.type = VAL_NODESET;

            template_context *new_context = memory_allocator_new(sizeof(template_context));
            new_context->context = context->context;
            new_context->instruction = i->children;
            new_context->result = parameter->extra.v.nodeset;
            new_context->document_node = context->document_node;
            new_context->local_variables = context->local_variables;
            new_context->task_mode = SINGLE;

            apply_xslt_template(new_context);
        }
        else
        {
            xpath_eval_node(context->context, context->local_variables, context->document_node, select, &(parameter->extra));
        }
    }

    XMLNODE *local_variables = xml_new_node(context->context, NULL, EMPTY_NODE);
    XMLNODE *result = xml_append_child(context->context, context->result, EMPTY_NODE);

    template_context *new_context = memory_allocator_new(sizeof(template_context));
    new_context->context = context->context;
    new_context->instruction = template;
    new_context->result = result;
    new_context->document_node = context->document_node;
    new_context->parameters = parameters;
    new_context->local_variables = local_variables;
    new_context->workers = context->workers;
    new_context->task_mode = context->task_mode;

    template_task_run(instruction, new_context, apply_xslt_template);
}

void instruction_apply_templates(template_context *context, XMLNODE *instruction)
{
    XMLNODE *selection;
    XMLSTRING select = xml_get_attr(instruction, xsl_a_select);
    if (select)
    {
        if (!instruction->compiled)
        {
            instruction->compiled = xpath_find_expr(context->context, select);
        }
        selection = xpath_eval_selection(context->context, context->local_variables, context->document_node, instruction->compiled);
    }
    else
    {
        selection = context->document_node->children;
    }

    // process template parameters
    XMLNODE *parameters = NULL;
    for (XMLNODE *child = instruction->children; child; child = child->next)
    {
        if (child->type == EMPTY_NODE) continue;

        if (xmls_equals(child->name, xsl_withparam))
        {
            XMLSTRING name = xml_get_attr(child, xsl_a_name);
            XMLNODE *parameter = xml_new_node(context->context, name, ATTRIBUTE_NODE);
            parameter->next = parameters;
            parameters = parameter;

            XMLSTRING expr = xml_get_attr(child, xsl_a_select);
            if (!expr)
            {
                parameter->extra.v.nodeset = xml_new_node(context->context, NULL, EMPTY_NODE);
                parameter->extra.type = VAL_NODESET;

                template_context *new_context = memory_allocator_new(sizeof(template_context));
                new_context->context = context->context;
                new_context->instruction = child->children;
                new_context->result = parameter->extra.v.nodeset;
                new_context->document_node = context->document_node;
                new_context->local_variables = context->local_variables;
                new_context->task_mode = SINGLE;

                apply_xslt_template(new_context);
            }
            else
            {
                xpath_eval_node(context->context, context->local_variables, context->document_node, expr, &(parameter->extra));
            }

            continue;
        }

        if (!xmls_equals(child->name, xsl_sort)) break;
        selection = xpath_sort_selection(context->context, context->local_variables, selection, child);
    }

    XMLNODE *local_variables = xml_new_node(context->context, NULL, EMPTY_NODE);
    XMLSTRING mode = xml_get_attr(instruction, xsl_a_mode);
    for (XMLNODE *node = selection; node; node = node->next)
    {
        XMLNODE *result = xml_append_child(context->context, context->result, EMPTY_NODE);

        template_context *new_context = memory_allocator_new(sizeof(template_context));
        new_context->context = context->context;
        new_context->result = result;
        new_context->document_node = node;
        new_context->parameters = parameters ? parameters : context->parameters;
        new_context->local_variables = local_variables;
        new_context->mode = mode;
        new_context->workers = context->workers;
        new_context->task_mode = context->task_mode;

        is_node_parallel(node) ? template_task_run(instruction, new_context, process_one_node) : process_one_node(new_context);
    }
}

void instruction_attribute(template_context *context, XMLNODE *instruction)
{
    XMLSTRING name = xml_get_attr(instruction, xsl_a_name);
    XMLSTRING attribute_name = xml_process_string(context->context, context->local_variables, context->document_node, name);
    XMLSTRING attribute_value = xml_eval_string(context->context, context->local_variables, context->document_node, instruction->children);

    // remove extra ws at start
    char *p;
    for(p = attribute_value->s; *p && x_is_ws(*p); ++p)  
        ;

    XMLNODE *parent = context->result;
    while(parent && parent->type == EMPTY_NODE) {
        parent = parent->parent;
    }
    if(!parent) return;

    XMLNODE *tmp;
    for(tmp = parent->attributes; tmp; tmp = tmp->next) {
        if(xmls_equals(tmp->name, attribute_name)) break;
    }

    if(!tmp) {
        tmp = xml_new_node(context->context, NULL, ATTRIBUTE_NODE);
        tmp->name = attribute_name;
        tmp->next = parent->attributes;
        // here means we don't need processing
        tmp->flags = XML_FLAG_NOESCAPE;
        parent->attributes = tmp;
    }
    tmp->content = xmls_new_string_literal(p);
}

void instruction_element(template_context *context, XMLNODE *instruction)
{
    XMLSTRING name = xml_get_attr(instruction, xsl_a_name);
    XMLSTRING element_name = xml_process_string(context->context, context->local_variables, context->document_node, name);
    
    XMLNODE *element = xml_append_child(context->context, context->result, ELEMENT_NODE);
    element->name = element_name;

    XMLSTRING namespace = xml_get_attr(instruction, xsl_a_xmlns);
    if (namespace)
    {
        XMLSTRING element_namespace = xml_process_string(context->context, context->local_variables, context->document_node, namespace);
        XMLNODE *attribute = xml_new_node(context->context, xsl_a_xmlns, ATTRIBUTE_NODE);
        attribute->content = element_namespace;
        element->attributes = attribute;
    }

    template_context *new_context = memory_allocator_new(sizeof(template_context));
    new_context->context = context->context;
    new_context->instruction = instruction->children;
    new_context->result = element;
    new_context->document_node = context->document_node;
    new_context->parameters = context->parameters;
    new_context->local_variables = copy_variables(context->context, context->local_variables);
    new_context->workers = context->workers;
    new_context->task_mode = context->task_mode;

    template_task_run(instruction, new_context, apply_xslt_template);
}

void instruction_if(template_context *context, XMLNODE *instruction)
{
    if (!instruction->compiled)
    {
        XMLSTRING test = xml_get_attr(instruction, xsl_a_test);
        instruction->compiled = xpath_find_expr(context->context, test);
    }
    
    if (xpath_eval_boolean(context->context, context->local_variables, context->document_node, instruction->compiled))
    {
        template_context *new_context = memory_allocator_new(sizeof(template_context));
        new_context->context = context->context;
        new_context->instruction = instruction->children;
        new_context->result = context->result;
        new_context->document_node = context->document_node;
        new_context->parameters = context->parameters;
        new_context->local_variables = context->local_variables;
        new_context->workers = context->workers;
        new_context->task_mode = context->task_mode;

        apply_xslt_template(new_context);
    }
}

void instruction_choose(template_context *context, XMLNODE *instruction)
{
    XMLNODE *otherwise = NULL;
    XMLNODE *i;
    for (i = instruction->children; i; i = i->next)
    {
        if (xmls_equals(i->name, xsl_otherwise))
        {
            otherwise = i;
        }
        
        if (xmls_equals(i->name, xsl_when))
        {
            if (!i->compiled)
            {
                XMLSTRING test = xml_get_attr(i, xsl_a_test);
                i->compiled = xpath_find_expr(context->context, test);
            }
            
            if (xpath_eval_boolean(context->context, context->local_variables, context->document_node, i->compiled))
            {
                template_context *new_context = memory_allocator_new(sizeof(template_context));
                new_context->context = context->context;
                new_context->instruction = i->children;
                new_context->result = context->result;
                new_context->document_node = context->document_node;
                new_context->parameters = context->parameters;
                new_context->local_variables = context->local_variables;
                new_context->workers = context->workers;
                new_context->task_mode = context->task_mode;

                apply_xslt_template(new_context);
                break;
            }
        }
    }

    // no when clause selected -- go for otherwise
    if (i == NULL && otherwise)
    {
        template_context *new_context = memory_allocator_new(sizeof(template_context));
        new_context->context = context->context;
        new_context->instruction = otherwise->children;
        new_context->result = context->result;
        new_context->document_node = context->document_node;
        new_context->parameters = context->parameters;
        new_context->local_variables = context->local_variables;
        new_context->workers = context->workers;
        new_context->task_mode = context->task_mode;

        apply_xslt_template(new_context);
    }
}

void instruction_param(template_context *context, XMLNODE *instruction)
{
    XMLSTRING name = xml_get_attr(instruction, xsl_a_name);
    XMLSTRING select = xml_get_attr(instruction, xsl_a_select);
    if (!xpath_in_selection(context->parameters, name) && (select || instruction->children))
    {
        do_local_var(context->context, context->local_variables, context->document_node, instruction);
    }
    else
    {
        add_local_var(context->context, context->local_variables, name, context->parameters);
    }
}

void instruction_for_each(template_context *context, XMLNODE *instruction)
{
    if (!instruction->compiled)
    {
        XMLSTRING select = xml_get_attr(instruction, xsl_a_select);
        instruction->compiled = xpath_find_expr(context->context, select);
    }
    XMLNODE *selection = xpath_eval_selection(context->context, context->local_variables, context->document_node, instruction->compiled);

    // check if sorting exists
    XMLNODE *child;
    for (child = instruction->children; child; child = child->next)
    {
        if (child->type == EMPTY_NODE) continue;
        if (!xmls_equals(child->name, xsl_sort)) break;
        selection = xpath_sort_selection(context->context, context->local_variables, selection, child);
    }
    
    for (XMLNODE *node = selection; node; node = node->next)
    {
        XMLNODE *result = xml_append_child(context->context, context->result, EMPTY_NODE);

        template_context *new_context = memory_allocator_new(sizeof(template_context));
        new_context->context = context->context;
        new_context->instruction = child;
        new_context->result = result;
        new_context->document_node = node;
        new_context->parameters = context->parameters;
        new_context->local_variables = copy_variables(context->context, context->local_variables);
        new_context->workers = context->workers;
        new_context->task_mode = context->task_mode;

        is_node_parallel(node) ? template_task_run(instruction, new_context, apply_xslt_template) : apply_xslt_template(new_context);
    }
}

void instruction_copy_of(template_context *context, XMLNODE *instruction)
{
    XMLSTRING select = xml_get_attr(instruction, xsl_a_select);
    RVALUE rv;
    xpath_eval_expression(context->context, context->local_variables, context->document_node, select, &rv);
    if (rv.type != VAL_NODESET)
    {
        char *text = rval2string(&rv);
        if (text) 
        {
            XMLNODE *node = xml_append_child(context->context, context->result, TEXT_NODE);
            node->content = xmls_new_string_literal(text);
        }
    } 
    else
    {
        for (XMLNODE *i = rv.v.nodeset; i; i = i->next)
        {
            copy_node_to_result_rec(context->context, context->local_variables, context->document_node, context->result, i);
        }
        rval_free(&rv);
    }
}

void instruction_variable(template_context *context, XMLNODE *instruction)
{
    do_local_var(context->context, context->local_variables, context->document_node, instruction);
}

void instruction_value_of(template_context *context, XMLNODE *instruction)
{
    if (!instruction->compiled)
    {
        XMLSTRING select = xml_get_attr(instruction, xsl_a_select);
        instruction->compiled = xpath_find_expr(context->context, select);
    }

    char *content = xpath_eval_string(context->context, context->local_variables, context->document_node, instruction->compiled);
    if (content)
    {
        XMLNODE *tmp = xml_append_child(context->context, context->result, TEXT_NODE);
        tmp->content = xmls_new_string_literal(content);
        tmp->flags |= instruction->flags & XML_FLAG_NOESCAPE;
    }
}

void instruction_text(template_context *context, XMLNODE *instruction)
{
    for (XMLNODE *i = instruction->children; i; i = i->next)
    {
        if (i->type == TEXT_NODE)
        {
            XMLNODE *node = xml_append_child(context->context, context->result, TEXT_NODE);
            node->content = xmls_new_string_literal(i->content->s);
            node->flags |= instruction->flags & XML_FLAG_NOESCAPE;
        }
    }
}

void instruction_copy(template_context *context, XMLNODE *instruction)
{
    XMLNODE *document_node = context->document_node;
    XMLNODE *result = context->result;
    if (document_node->type == ATTRIBUTE_NODE)
    {
        XMLNODE *node = result;
        if (node->type == EMPTY_NODE) node = node->parent;

        if (node != NULL && node->type == ELEMENT_NODE)
        {
            XMLNODE *attribute = xml_new_node(context->context, document_node->name, ATTRIBUTE_NODE);
            attribute->content = document_node->content;

            if (node->attributes == NULL)
            {
                node->attributes = attribute;
            }
            else
            {
                node->attributes->next = attribute;
            }
        }
    }
    else if (document_node->type == TEXT_NODE)
    {
        result = xml_append_child(context->context, result, TEXT_NODE);
        result->content = document_node->content;
    }
    else if (document_node->type == ELEMENT_NODE)
    {
        result = xml_append_child(context->context, result, ELEMENT_NODE);
        result->name = document_node->name;
    }

    if (instruction->children)
    {
        template_context *new_context = memory_allocator_new(sizeof(template_context));
        new_context->context = context->context;
        new_context->instruction = instruction->children;
        new_context->result = result;
        new_context->document_node = document_node;
        new_context->parameters = context->parameters;
        new_context->local_variables = context->local_variables;
        new_context->workers = context->workers;
        new_context->task_mode = context->task_mode;

        apply_xslt_template(new_context);
    }
}

void instruction_message(template_context *context, XMLNODE *instruction)
{
    XMLSTRING msg = xml_eval_string(context->context, context->local_variables, context->document_node, instruction->children);
    if (msg)
    {
        info("instruction_message:: message %s", msg->s);
    }
    else
    {
        error("instruction_message:: fail to get message");
    }
}

void instruction_number(template_context *context, XMLNODE *instruction)
{
    XMLNODE *node = xml_append_child(context->context, context->result, TEXT_NODE);
    node->content = xmls_new_string_literal("1.");
}

void instruction_comment(template_context *context, XMLNODE *instruction)
{
    XMLNODE *node = xml_append_child(context->context, context->result, COMMENT_NODE);
    node->content = xml_eval_string(context->context, context->local_variables, context->document_node, instruction->children);
}

void instruction_processing_instruction(template_context *context, XMLNODE *instruction)
{
    XMLNODE *node = xml_append_child(context->context, context->result, PI_NODE);
    node->name = xml_get_attr(instruction, xsl_a_name);
    node->content = xml_eval_string(context->context, context->local_variables, context->document_node, instruction->children);
}

void instruction_add(XMLSTRING name, void(*function)(template_context *, XMLNODE *))
{
    dict_add(instructions, name, function);
}

void instructions_create()
{
    info("instructions_create");

    instructions = dict_new(32);

    instruction_add(xsl_call, instruction_call_template);
    instruction_add(xsl_apply, instruction_apply_templates);
    instruction_add(xsl_attribute, instruction_attribute);
    instruction_add(xsl_element, instruction_element);
    instruction_add(xsl_if, instruction_if);
    instruction_add(xsl_choose, instruction_choose);
    instruction_add(xsl_param, instruction_param);
    instruction_add(xsl_foreach, instruction_for_each);
    instruction_add(xsl_copyof, instruction_copy_of);
    instruction_add(xsl_var, instruction_variable);
    instruction_add(xsl_value_of, instruction_value_of);
    instruction_add(xsl_text, instruction_text);
    instruction_add(xsl_copy, instruction_copy);
    instruction_add(xsl_message, instruction_message);
    instruction_add(xsl_number, instruction_number);
    instruction_add(xsl_comment, instruction_comment);
    instruction_add(xsl_pi, instruction_processing_instruction);
}

void instructions_release()
{
    info("instructions_release");

    dict_free(instructions);
    instructions = NULL;
}

int instructions_is_xsl(XMLNODE *instruction)
{
    XMLSTRING name = instruction->name;
    if (name == NULL || name->len < 4) return 0;

    return name->s[0] == 'x' && name->s[1] == 's' && name->s[2] == 'l' && name->s[3] == ':';
}

void instructions_process(template_context *context, XMLNODE *instruction)
{
    XMLSTRING name = instruction->name;
    void(*function)(template_context *, XMLNODE *) = dict_find(instructions, name);
    if (function == NULL)
    {
        error("instructions_process:: instruction <%s> not supported!", name->s);
        return;
    }

    function(context, instruction);
}

void instructions_set_parallel(TRANSFORM_CONTEXT *pctx)
{
    pctx->parallel_instructions = dict_new(32);
    dict_add(pctx->parallel_instructions, xsl_apply, xsl_apply);
    dict_add(pctx->parallel_instructions, xsl_foreach, xsl_foreach);
}
