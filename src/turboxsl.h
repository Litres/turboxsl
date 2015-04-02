#ifndef TURBOXSL_H_
#define TURBOXSL_H_

struct _xmlnode;
typedef struct _xmlnode XMLNODE;

struct _globaldata;
typedef struct _globaldata XSLTGLOBALDATA;

struct _context;
typedef struct _context TRANSFORM_CONTEXT;

XSLTGLOBALDATA *XSLTInit(void *interpreter);
void XSLTEnableExternalCache(XSLTGLOBALDATA *data, char *server_list);
void XSLTEnd(XSLTGLOBALDATA *data);

TRANSFORM_CONTEXT *XSLTNewProcessor(XSLTGLOBALDATA *data, char *stylesheet);
void XSLTCreateThreadPool(TRANSFORM_CONTEXT *ctx, unsigned int size);
void XSLTSetCacheKeyPrefix(TRANSFORM_CONTEXT *ctx, char *prefix);
XMLNODE *XSLTProcess(TRANSFORM_CONTEXT *ctx, XMLNODE *xml);
void XSLTFreeProcessor(TRANSFORM_CONTEXT *ctx);

XMLNODE *XMLParse(XSLTGLOBALDATA *data, char *text);
XMLNODE *XMLParseFile(XSLTGLOBALDATA *data, char *filename);
char *XMLOutput(TRANSFORM_CONTEXT *ctx, XMLNODE *xml);
void XMLOutputFile(TRANSFORM_CONTEXT *ctx, XMLNODE *xml, char *filename);
XMLNODE *XMLFindNodes(TRANSFORM_CONTEXT *ctx, XMLNODE *xml, char *expression);
char *XMLStringValue(XMLNODE *xml);
char **XMLAttributes(XMLNODE *xml);
void XMLFreeDocument(XMLNODE *node);

void set_ctx_global_var(TRANSFORM_CONTEXT *pctx, char *name, char *content);
void set_global_var(XSLTGLOBALDATA *pctx, char *name, char *content);

void register_function(XSLTGLOBALDATA *pctx, char *fname, char *(*callback)(void *fun,char **args,void *interpreter), void (*fun)());
char *xml_strdup(char *s);

#endif
