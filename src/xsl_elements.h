#ifndef XSL_ELEMENTS_H_
#define XSL_ELEMENTS_H_

#include "ltr_xsl.h"

XMLSTRING xsl_template;
XMLSTRING xsl_apply;
XMLSTRING xsl_call;
XMLSTRING xsl_if;
XMLSTRING xsl_choose;
XMLSTRING xsl_when;
XMLSTRING xsl_otherwise;
XMLSTRING xsl_value_of;
XMLSTRING xsl_include;
XMLSTRING xsl_element;
XMLSTRING xsl_attribute;
XMLSTRING xsl_text;
XMLSTRING xsl_pi;
XMLSTRING xsl_comment;
XMLSTRING xsl_number;
XMLSTRING xsl_foreach;
XMLSTRING xsl_copy;
XMLSTRING xsl_sort;
XMLSTRING xsl_output;
XMLSTRING xsl_copyof;
XMLSTRING xsl_message;
XMLSTRING xsl_var;
XMLSTRING xsl_key;
XMLSTRING xsl_decimal;
XMLSTRING xsl_withparam;
XMLSTRING xsl_param;
XMLSTRING xsl_import;

XMLSTRING xsl_a_match;
XMLSTRING xsl_a_select;
XMLSTRING xsl_a_test;
XMLSTRING xsl_a_href;
XMLSTRING xsl_a_name;
XMLSTRING xsl_a_mode;
XMLSTRING xsl_a_escaping;
XMLSTRING xsl_a_method;
XMLSTRING xsl_a_omitxml;
XMLSTRING xsl_a_standalone;
XMLSTRING xsl_a_media;
XMLSTRING xsl_a_encoding;
XMLSTRING xsl_a_dtpublic;
XMLSTRING xsl_a_dtsystem;
XMLSTRING xsl_a_xmlns;
XMLSTRING xsl_a_use;
XMLSTRING xsl_a_datatype;
XMLSTRING xsl_a_order;
XMLSTRING xsl_a_caseorder;

XMLSTRING xsl_a_decimal_separator;
XMLSTRING xsl_a_grouping_separator;
XMLSTRING xsl_a_percent;
XMLSTRING xsl_a_zero_digit;
XMLSTRING xsl_a_digit;
XMLSTRING xsl_a_pattern_separator;
XMLSTRING xsl_a_infinity;
XMLSTRING xsl_a_NaN;
XMLSTRING xsl_a_minus_sign;

XMLSTRING xsl_s_xml;
XMLSTRING xsl_s_html;
XMLSTRING xsl_s_text;
XMLSTRING xsl_s_yes;
XMLSTRING xsl_s_number;
XMLSTRING xsl_s_descending;
XMLSTRING xsl_s_lower_first;
XMLSTRING xsl_s_script;
XMLSTRING xsl_s_node;

XMLSTRING xsl_s_head;
XMLSTRING xsl_s_img;
XMLSTRING xsl_s_meta;
XMLSTRING xsl_s_hr;
XMLSTRING xsl_s_br;
XMLSTRING xsl_s_link;
XMLSTRING xsl_s_input;

void xsl_elements_setup();

#endif
