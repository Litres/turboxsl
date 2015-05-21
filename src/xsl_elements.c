#include "xsl_elements.h"

void xsl_elements_setup()
{
    xsl_template   = xmls_new_string_literal("xsl:template");
    xsl_apply      = xmls_new_string_literal("xsl:apply-templates");
    xsl_call       = xmls_new_string_literal("xsl:call-template");
    xsl_if         = xmls_new_string_literal("xsl:if");
    xsl_choose     = xmls_new_string_literal("xsl:choose");
    xsl_when       = xmls_new_string_literal("xsl:when");
    xsl_otherwise  = xmls_new_string_literal("xsl:otherwise");
    xsl_element    = xmls_new_string_literal("xsl:element");
    xsl_attribute  = xmls_new_string_literal("xsl:attribute");
    xsl_text       = xmls_new_string_literal("xsl:text");
    xsl_pi         = xmls_new_string_literal("xsl:processing-instruction");
    xsl_comment    = xmls_new_string_literal("xsl:comment");
    xsl_number     = xmls_new_string_literal("xsl:number");
    xsl_foreach    = xmls_new_string_literal("xsl:for-each");
    xsl_copy       = xmls_new_string_literal("xsl:copy");
    xsl_copyof     = xmls_new_string_literal("xsl:copy-of");
    xsl_message    = xmls_new_string_literal("xsl:message");
    xsl_var        = xmls_new_string_literal("xsl:variable");
    xsl_param      = xmls_new_string_literal("xsl:param");
    xsl_withparam  = xmls_new_string_literal("xsl:with-param");
    xsl_decimal    = xmls_new_string_literal("xsl:decimal-format");
    xsl_sort       = xmls_new_string_literal("xsl:sort");
    xsl_value_of   = xmls_new_string_literal("xsl:value-of");
    xsl_pi         = xmls_new_string_literal("xsl:processing-instruction");
    xsl_output     = xmls_new_string_literal("xsl:output");
    xsl_key        = xmls_new_string_literal("xsl:key");
    xsl_include    = xmls_new_string_literal("xsl:include");
    xsl_import     = xmls_new_string_literal("xsl:import");

    xsl_a_match    = xmls_new_string_literal("match");
    xsl_a_select   = xmls_new_string_literal("select");
    xsl_a_href     = xmls_new_string_literal("href");
    xsl_a_name     = xmls_new_string_literal("name");
    xsl_a_test     = xmls_new_string_literal("test");
    xsl_a_mode     = xmls_new_string_literal("mode");
    xsl_a_escaping = xmls_new_string_literal("disable-output-escaping");
    xsl_a_method   = xmls_new_string_literal("method");
    xsl_a_omitxml  = xmls_new_string_literal("omit-xml-declaration");
    xsl_a_standalone = xmls_new_string_literal("standalone");
    xsl_a_media    = xmls_new_string_literal("media-type");
    xsl_a_encoding = xmls_new_string_literal("encoding");
    xsl_a_dtpublic = xmls_new_string_literal("doctype-public");
    xsl_a_dtsystem = xmls_new_string_literal("doctype-system");
    xsl_a_xmlns    = xmls_new_string_literal("xmlns");
    xsl_a_use      = xmls_new_string_literal("use");
    xsl_a_datatype = xmls_new_string_literal("data-type");
    xsl_a_order    = xmls_new_string_literal("order");
    xsl_a_caseorder = xmls_new_string_literal("case-order");
    xsl_a_fork     = xmls_new_string_literal("fork");

    xsl_a_decimal_separator = xmls_new_string_literal("decimal-separator");
    xsl_a_grouping_separator = xmls_new_string_literal("grouping-separator");
    xsl_a_percent = xmls_new_string_literal("percent");
    xsl_a_zero_digit = xmls_new_string_literal("zero-digit");
    xsl_a_digit = xmls_new_string_literal("digit");
    xsl_a_pattern_separator = xmls_new_string_literal("pattern-separator");
    xsl_a_infinity = xmls_new_string_literal("infinity");
    xsl_a_NaN = xmls_new_string_literal("NaN");
    xsl_a_minus_sign = xmls_new_string_literal("minus-sign");

    xsl_s_xml = xmls_new_string_literal("xml");
    xsl_s_html = xmls_new_string_literal("html");
    xsl_s_text = xmls_new_string_literal("text");
    xsl_s_yes = xmls_new_string_literal("yes");
    xsl_s_no = xmls_new_string_literal("no");
    xsl_s_number = xmls_new_string_literal("number");
    xsl_s_descending = xmls_new_string_literal("descending");
    xsl_s_lower_first = xmls_new_string_literal("lower-first");
    xsl_s_script = xmls_new_string_literal("script");
    xsl_s_node = xmls_new_string_literal("node");
    xsl_s_root = xmls_new_string_literal("root");
    xsl_s_deny = xmls_new_string_literal("deny");

    xsl_s_head = xmls_new_string_literal("head");
    xsl_s_img = xmls_new_string_literal("img");
    xsl_s_meta = xmls_new_string_literal("meta");
    xsl_s_hr = xmls_new_string_literal("hr");
    xsl_s_br = xmls_new_string_literal("br");
    xsl_s_link = xmls_new_string_literal("link");
    xsl_s_input = xmls_new_string_literal("input");
}
