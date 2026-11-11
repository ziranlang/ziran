/* Run the Android syscall fixture against Linux libc. Bionic exposes __errno;
 * the host adapter returns the real thread-local Linux errno storage. Android
 * cross-links use Bionic directly and never compile this adapter. */
#include <errno.h>
int *__errno(void) { return &errno; }
