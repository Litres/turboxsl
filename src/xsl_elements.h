#ifndef XSL_ELEMENTS_H_
#define XSL_ELEMENTS_H_

#include "ltr_xsl.h"

extern XMLSTRING xsl_template;
extern XMLSTRING xsl_apply;
extern XMLSTRING xsl_call;
extern XMLSTRING xsl_if;
extern XMLSTRING xsl_choose;
extern XMLSTRING xsl_when;
extern XMLSTRING xsl_otherwise;
extern XMLSTRING xsl_value_of;
extern XMLSTRING xsl_include;
extern XMLSTRING xsl_element;
extern XMLSTRING xsl_attribute;
extern XMLSTRING xsl_text;
extern XMLSTRING xsl_pi;
extern XMLSTRING xsl_comment;
extern XMLSTRING xsl_number;
extern XMLSTRING xsl_foreach;
extern XMLSTRING xsl_copy;
extern XMLSTRING xsl_sort;
extern XMLSTRING xsl_output;
extern XMLSTRING xsl_copyof;
extern XMLSTRING xsl_message;
extern XMLSTRING xsl_var;
extern XMLSTRING xsl_key;
extern XMLSTRING xsl_decimal;
extern XMLSTRING xsl_withparam;
extern XMLSTRING xsl_param;
extern XMLSTRING xsl_import;

extern XMLSTRING xsl_a_match;
extern XMLSTRING xsl_a_select;
extern XMLSTRING xsl_a_test;
extern XMLSTRING xsl_a_href;
extern XMLSTRING xsl_a_name;
extern XMLSTRING xsl_a_mode;
extern XMLSTRING xsl_a_escaping;
extern XMLSTRING xsl_a_method;
extern XMLSTRING xsl_a_omitxml;
extern XMLSTRING xsl_a_standalone;
extern XMLSTRING xsl_a_media;
extern XMLSTRING xsl_a_encoding;
extern XMLSTRING xsl_a_dtpublic;
extern XMLSTRING xsl_a_dtsystem;
extern XMLSTRING xsl_a_xmlns;
extern XMLSTRING xsl_a_use;
extern XMLSTRING xsl_a_datatype;
extern XMLSTRING xsl_a_order;
extern XMLSTRING xsl_a_caseorder;
extern XMLSTRING xsl_a_fork;

extern XMLSTRING xsl_a_decimal_separator;
extern XMLSTRING xsl_a_grouping_separator;
extern XMLSTRING xsl_a_percent;
extern XMLSTRING xsl_a_zero_digit;
extern XMLSTRING xsl_a_digit;
extern XMLSTRING xsl_a_pattern_separator;
extern XMLSTRING xsl_a_infinity;
extern XMLSTRING xsl_a_NaN;
extern XMLSTRING xsl_a_minus_sign;

extern XMLSTRING xsl_s_xml;
extern XMLSTRING xsl_s_html;
extern XMLSTRING xsl_s_text;
extern XMLSTRING xsl_s_yes;
extern XMLSTRING xsl_s_no;
extern XMLSTRING xsl_s_number;
extern XMLSTRING xsl_s_descending;
extern XMLSTRING xsl_s_lower_first;
extern XMLSTRING xsl_s_script;
extern XMLSTRING xsl_s_node;
extern XMLSTRING xsl_s_root;
extern XMLSTRING xsl_s_deny;
extern XMLSTRING xsl_s_slash;

extern XMLSTRING xsl_s_head;
extern XMLSTRING xsl_s_img;
extern XMLSTRING xsl_s_meta;
extern XMLSTRING xsl_s_hr;
extern XMLSTRING xsl_s_br;
extern XMLSTRING xsl_s_link;
extern XMLSTRING xsl_s_input;

extern XMLSTRING xsl_s_red;
extern XMLSTRING xsl_s_green;
extern XMLSTRING xsl_s_yellow;

void xsl_elements_setup();

#endif
