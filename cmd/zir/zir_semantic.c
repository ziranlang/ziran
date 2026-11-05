#include "zir_semantic.h"
#include "zir_sha256.h"
#include "zir_text.h"
#include <stdio.h>
#include <string.h>

void SemanticId(const ZirModule *module, const char *kind, const char *name,
                const char *signature, const char *result, char id[68])
{
    Sha256 hash; unsigned char bytes[32];
    const char *parts[] = {"ziran.symbol.1", module->name, kind, name, signature, result};
    static const char hex[] = "0123456789abcdef";
    Sha256Begin(&hash);
    for(size_t i = 0; i < sizeof(parts)/sizeof(*parts); i++)
        Sha256Add(&hash, parts[i] ? parts[i] : "", strlen(parts[i] ? parts[i] : "") + 1);
    Sha256End(&hash, bytes);
    memcpy(id, "zi:", 3);
    for(size_t i = 0; i < 32; i++) { id[3+i*2] = hex[bytes[i]>>4]; id[4+i*2] = hex[bytes[i]&15]; }
    id[67] = 0;
}

void FunctionSemanticId(const ZirModule *module, const ZirFunction *function, char id[68])
{
    char normalized[32];
    const char *name = function->overload_name[0] ? function->overload_name : function->name;
    const char *operator = OperatorOfProcedure(name);
    if(operator) { snprintf(normalized, sizeof(normalized), "operator %s", operator); name = normalized; }
    SemanticId(module, "function", name, FunctionArgs(function), function->return_type, id);
}
