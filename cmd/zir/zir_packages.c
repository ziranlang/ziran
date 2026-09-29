#include "zir_packages.h"
#include "zir_diagnostic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct PackageRoot {
    char *id;
    char *path;
} PackageRoot;

typedef struct PackageModule {
    char *id;
    char *name;
    char *path;
    char identity[ZIR_NAME_MAX];
} PackageModule;

typedef struct PackageDependency {
    char *owner;
    char *visible;
    char *target;
    char *exported;
} PackageDependency;

/* A short name exported by more than one direct dependency. Importing it
 * reports the package-qualified spellings instead of choosing one. */
typedef struct PackageAmbiguous {
    char *owner;
    char *visible;
    char *choices;
} PackageAmbiguous;

struct ZirPackageMap {
    PackageRoot *roots;
    int root_count;
    PackageModule *modules;
    int module_count;
    PackageDependency *dependencies;
    int dependency_count;
    PackageAmbiguous *ambiguous;
    int ambiguous_count;
};

static char *field(char **cursor)
{
    char *start = *cursor;
    char *tab = strchr(start, '\t');
    if(tab != NULL) {
        *tab = '\0';
        *cursor = tab + 1;
    } else {
        char *end = strchr(start, '\n');
        if(end != NULL) *end = '\0';
        *cursor = NULL;
    }
    return start;
}

void PackageMapFree(ZirPackageMap *map)
{
    if(map == NULL) return;
    for(int i = 0; i < map->root_count; i++) {
        free(map->roots[i].id);
        free(map->roots[i].path);
    }
    for(int i = 0; i < map->module_count; i++) {
        free(map->modules[i].id);
        free(map->modules[i].name);
        free(map->modules[i].path);
    }
    for(int i = 0; i < map->dependency_count; i++) {
        free(map->dependencies[i].owner);
        free(map->dependencies[i].visible);
        free(map->dependencies[i].target);
        free(map->dependencies[i].exported);
    }
    for(int i = 0; i < map->ambiguous_count; i++) {
        free(map->ambiguous[i].owner);
        free(map->ambiguous[i].visible);
        free(map->ambiguous[i].choices);
    }
    free(map->roots);
    free(map->modules);
    free(map->dependencies);
    free(map->ambiguous);
    free(map);
}

ZirPackageMap *PackageMapLoad(const char *path)
{
    FILE *file = fopen(path, "r");
    if(file == NULL) {
        Diagnostic(Span(path, 1, 1), "package.map", "cannot open package map");
        return NULL;
    }
    ZirPackageMap *map = calloc(1, sizeof(*map));
    char *line = NULL;
    size_t capacity = 0;
    int okay = map != NULL;
    while(okay && getline(&line, &capacity, file) >= 0) {
        char *cursor = line;
        char *kind = field(&cursor);
        if(cursor == NULL) { okay = 0; break; }
        if(strcmp(kind, "P") == 0) {
            PackageRoot *items = realloc(map->roots,
                (size_t)(map->root_count + 1) * sizeof(*items));
            if(items == NULL) { okay = 0; break; }
            map->roots = items;
            PackageRoot *item = &map->roots[map->root_count++];
            item->id = strdup(field(&cursor));
            item->path = cursor == NULL ? NULL : strdup(field(&cursor));
            okay = item->id != NULL && item->path != NULL;
        } else if(strcmp(kind, "M") == 0) {
            PackageModule *items = realloc(map->modules,
                (size_t)(map->module_count + 1) * sizeof(*items));
            if(items == NULL) { okay = 0; break; }
            map->modules = items;
            PackageModule *item = &map->modules[map->module_count++];
            memset(item, 0, sizeof(*item));
            item->id = strdup(field(&cursor));
            item->name = cursor == NULL ? NULL : strdup(field(&cursor));
            item->path = cursor == NULL ? NULL : strdup(field(&cursor));
            okay = item->id != NULL && item->name != NULL && item->path != NULL;
            if(okay) {
                int n = strcmp(item->id, "root") == 0 ?
                    snprintf(item->identity, sizeof(item->identity), "%s", item->name) :
                    snprintf(item->identity, sizeof(item->identity), "%s_%s", item->id, item->name);
                okay = n > 0 && n < (int)sizeof(item->identity);
            }
        } else if(strcmp(kind, "D") == 0) {
            PackageDependency *items = realloc(map->dependencies,
                (size_t)(map->dependency_count + 1) * sizeof(*items));
            if(items == NULL) { okay = 0; break; }
            map->dependencies = items;
            PackageDependency *item = &map->dependencies[map->dependency_count++];
            memset(item, 0, sizeof(*item));
            item->owner = strdup(field(&cursor));
            item->visible = cursor == NULL ? NULL : strdup(field(&cursor));
            item->target = cursor == NULL ? NULL : strdup(field(&cursor));
            item->exported = cursor == NULL ? NULL : strdup(field(&cursor));
            okay = item->owner && item->visible && item->target && item->exported;
        } else if(strcmp(kind, "A") == 0) {
            PackageAmbiguous *items = realloc(map->ambiguous,
                (size_t)(map->ambiguous_count + 1) * sizeof(*items));
            if(items == NULL) { okay = 0; break; }
            map->ambiguous = items;
            PackageAmbiguous *item = &map->ambiguous[map->ambiguous_count++];
            memset(item, 0, sizeof(*item));
            item->owner = strdup(field(&cursor));
            item->visible = cursor == NULL ? NULL : strdup(field(&cursor));
            item->choices = cursor == NULL ? NULL : strdup(field(&cursor));
            okay = item->owner && item->visible && item->choices;
        } else okay = 0;
    }
    free(line);
    fclose(file);
    if(!okay) {
        Diagnostic(Span(path, 1, 1), "package.map", "invalid package map");
        PackageMapFree(map);
        return NULL;
    }
    return map;
}

const char *PackageOwner(const ZirPackageMap *map, const char *path)
{
    const char *owner = NULL;
    size_t best = 0;
    for(int i = 0; i < map->root_count; i++) {
        size_t length = strlen(map->roots[i].path);
        if(length > best && strncmp(path, map->roots[i].path, length) == 0 &&
           (path[length] == '/' || path[length] == '\0')) {
            owner = map->roots[i].id;
            best = length;
        }
    }
    return owner;
}

const char *PackageModuleName(const ZirPackageMap *map, const char *path)
{
    for(int i = 0; i < map->module_count; i++)
        if(strcmp(map->modules[i].path, path) == 0)
            return map->modules[i].identity;
    return NULL;
}

int PackageResolve(const ZirPackageMap *map, const char *owner,
                   const char *visible, char *path, size_t path_size,
                   char *identity, size_t identity_size)
{
    const char *target = owner;
    const char *name = visible;
    int local = 0;
    /* std/NAME always names the pinned standard library, even when the
     * package or a dependency has a module called NAME. */
    if(strncmp(visible, "std/", 4) == 0) {
        target = "std";
        name = visible + 4;
        local = 1;
    }
    for(int i = 0; !local && i < map->module_count; i++)
        if(strcmp(map->modules[i].id, owner) == 0 &&
           strcmp(map->modules[i].name, visible) == 0) {
            local = 1;
            break;
        }
    if(!local) {
        target = NULL;
        for(int i = 0; i < map->dependency_count; i++) {
            const PackageDependency *edge = &map->dependencies[i];
            if(strcmp(edge->owner, owner) == 0 &&
               strcmp(edge->visible, visible) == 0) {
                target = edge->target;
                name = edge->exported;
                break;
            }
        }
        if(target == NULL) {
            if(PackageAmbiguity(map, owner, visible) != NULL ||
               strchr(visible, '/') != NULL) return 0;
            target = "std";
            name = visible;
        }
    }
    for(int i = 0; i < map->module_count; i++) {
        const PackageModule *module = &map->modules[i];
        if(strcmp(module->id, target) != 0 ||
           strcmp(module->name, name) != 0) continue;
        if(snprintf(path, path_size, "%s", module->path) >= (int)path_size ||
           snprintf(identity, identity_size, "%s", module->identity) >=
           (int)identity_size) return 0;
        return 1;
    }
    return 0;
}

const char *PackageAmbiguity(const ZirPackageMap *map, const char *owner,
                             const char *visible)
{
    for(int i = 0; i < map->ambiguous_count; i++)
        if(strcmp(map->ambiguous[i].owner, owner) == 0 &&
           strcmp(map->ambiguous[i].visible, visible) == 0)
            return map->ambiguous[i].choices;
    return NULL;
}
