#include <stdio.h>
#include <stdlib.h>

#include <ltr_xsl.h>

int main(int n, char *args[]) {
    if (n < 4) {
        fprintf(stderr, "Usage: xsltproc <stylesheet.xsl> <input.xml> <output.html> <optional pool size>\n");
        exit(1);
    }

    XSLTGLOBALDATA *gctx = XSLTInit(NULL);
    if (!gctx) {
        fprintf(stderr, "error in initialization\n");
        exit(1);
    }

    TRANSFORM_CONTEXT *pctx = XSLTNewProcessor(gctx, args[1]);
    if (!pctx) {
        fprintf(stderr, "error in style processing\n");
        exit(1);
    }
    
    if (n == 5) {
        unsigned int size = (unsigned int)atoi(args[4]);
        if (size > 10) {
            fprintf(stderr, "pool size must be less then 10\n");
            exit(1);
        }
        XSLTCreateThreadPool(pctx, size);
    }

    XMLNODE *document = XMLParseFile(gctx, args[2]);
    if (!document) {
        fprintf(stderr, "error in document parsing\n");
        exit(1);
    }

    XMLNODE *result = XSLTProcess(pctx, document);
    if (!result) {
        fprintf(stderr, "error in transformation\n");
        exit(1);
    }

    XMLOutputFile(pctx, result, args[3]);

    XMLFreeDocument(result);
    XMLFreeDocument(document);
    XSLTFreeProcessor(pctx);
    XSLTEnd(gctx);

    return 0;
}
