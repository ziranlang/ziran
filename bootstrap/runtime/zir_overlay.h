#ifndef ZIR_OVERLAY_H
#define ZIR_OVERLAY_H
#include <stddef.h>
/* One immutable in-memory source replacement scoped to a loading operation.
 * Source is NUL-terminated and count excludes that terminator. The caller
 * owns both strings until End. Nested overlays are rejected. */
int SourceOverlayBegin(const char *path, const char *source, size_t count);
const char *SourceOverlayRead(const char *path, size_t *count);
void SourceOverlayRestrict(const char *root, const char *standard);
int SourceOverlayPermitRoot(const char *root);
int SourceOverlayAllowed(const char *path);
void SourceOverlayEnd(void);
#endif
