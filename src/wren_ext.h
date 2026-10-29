#ifndef __WREN_EXT_H
#define __WREN_EXT_H

#include "wren.h"
#include "app.h"

/** Initialize the Wren VM with card maker imports (allocator, write,
 * vfile module loader, foreign method binder). Idempotent. */
void wren_ext_init(void);

/** Read the script file named by the string variable `script_file`,
 * prepend the cardmaker prelude and interpret it. */
void wren_ext_eval_script_file(void);

#endif // __WREN_EXT_H
