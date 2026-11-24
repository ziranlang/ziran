/*
 * zir_zx_wasm_install.c - pulls the WebAssembly provider into the link.
 * The constructor of this always-linked object installs the provider;
 * native builds have none, and web builds define it in zir_zx_wasm_web.c.
 * The provider and its bridge live in cmd/compiler_zx_exec_wasm.zi.
 */

#include "ziran_bootstrap_runtime.h"

#ifndef __EMSCRIPTEN__
void
ZxWasmInstallBundledPlatform(void)
{
}
#endif

__attribute__((constructor))
static void
zx_wasm_install_at_load(void)
{
    ZxWasmInstallBundledPlatform();
}
