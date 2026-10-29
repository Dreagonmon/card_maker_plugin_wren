#include "app.h"
#include "wren_ext.h"
#include <wasmenv.h>

WASM_EXPORT("eval")
void eval(void) { wren_ext_eval_script_file(); }

WASM_EXPORT("_start")
void _start(void) { wren_ext_init(); }
