#include "wren_ext.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wasmenv.h>

#define SMALL_BUF_SIZE (256)

static WrenVM *vm = NULL;

// Built-in module "cardmaker": declares the foreign class and thin
// wrappers over the host variable API (see app.h).
static const char *CARDMAKER_MODULE_SOURCE =
    "foreign class cardmaker {\n"
    "  foreign static setStr(name, value)\n"
    "  foreign static setNum(name, value)\n"
    "  foreign static getStr(name)\n"
    "  foreign static getNum(name)\n"
    "  foreign static memFree()\n"
    "}\n";

// ---------------------------
//        Configuration
// ---------------------------

static void *reallocate(void *memory, size_t newSize, void *userData) {
  (void)userData;
  if (newSize == 0) {
    free(memory);
    return NULL;
  }
  return realloc(memory, newSize);
}

static void writeFn(WrenVM *v, const char *text) {
  (void)v;
  // No fputs/fflush in the minimal libc; emit raw UTF-8 bytes, the host
  // wasmenv decodes them back into codepoints.
  for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
    wasmenv_putc(*p);
  }
}

// Read a vfile fully into a malloc'ed NUL-terminated buffer. NULL on error.
static char *read_vfile(const char *path) {
  VFile vf = vfile_open(path);
  if (vf == NULL) return NULL;

  size_t cap = 4096, len = 0;
  char *buf = (char *)malloc(cap);
  if (buf == NULL) {
    vfile_close(vf);
    return NULL;
  }
  for (;;) {
    int ch = vfile_getc(vf);
    if (ch < 0) break;
    if (len + 1 >= cap) {
      cap *= 2;
      char *nb = (char *)realloc(buf, cap);
      if (nb == NULL) {
        free(buf);
        vfile_close(vf);
        return NULL;
      }
      buf = nb;
    }
    buf[len++] = (char)(ch & 0xFF);
  }
  vfile_close(vf);
  buf[len] = '\0';
  return buf;
}

// Load a module from the virtual filesystem: read the whole file into
// a malloc'ed, NUL-terminated buffer. Wren frees it with free().
static WrenLoadModuleResult loadModuleFn(WrenVM *v, const char *name) {
  (void)v;
  WrenLoadModuleResult result;
  result.onComplete = NULL;
  result.source = NULL;

  if (strcmp(name, "cardmaker") == 0) {
    size_t len = strlen(CARDMAKER_MODULE_SOURCE);
    char *buf = (char *)malloc(len + 1);
    if (buf != NULL) memcpy(buf, CARDMAKER_MODULE_SOURCE, len + 1);
    result.source = buf;
    return result;
  }

  result.source = read_vfile(name); // NULL => load error reported by wren
  return result;
}

// ---------------------------
//   "cardmaker" Foreign Class
// ---------------------------

static void cm_setStr(WrenVM *v) {
  const char *name = wrenGetSlotString(v, 1);
  const char *value = wrenGetSlotString(v, 2);
  set_string(name, value);
}

static void cm_setNum(WrenVM *v) {
  const char *name = wrenGetSlotString(v, 1);
  double value = wrenGetSlotDouble(v, 2);
  set_number(name, value);
}

static void cm_getStr(WrenVM *v) {
  const char *name = wrenGetSlotString(v, 1);
  int32_t len = get_string_length(name);
  if (len < 0) {
    wrenSetSlotNull(v, 0);
    return;
  }
  char small[SMALL_BUF_SIZE];
  char *buf = small;
  size_t need = (size_t)len + 1;
  if (need > SMALL_BUF_SIZE) {
    buf = (char *)malloc(need);
    if (buf == NULL) {
      wrenSetSlotString(v, 0, "out of memory");
      wrenAbortFiber(v, 0);
      return;
    }
  }
  if (get_string(name, buf, len + 1) < 0) {
    if (buf != small) free(buf);
    wrenSetSlotNull(v, 0);
    return;
  }
  wrenSetSlotString(v, 0, buf);
  if (buf != small) free(buf);
}

static void cm_getNum(WrenVM *v) {
  const char *name = wrenGetSlotString(v, 1);
  double value = get_number(name);
  if (isnan(value)) { // NaN: name not found
    wrenSetSlotNull(v, 0);
    return;
  }
  wrenSetSlotDouble(v, 0, value);
}

static void cm_memFree(WrenVM *v) {
  // No pair allocator in wren; report wasm linear memory pages * 64KiB
  // so scripts have *some* memory gauge.
  wrenSetSlotDouble(v, 0, (double)__builtin_wasm_memory_size(0) * 65536.0);
}

static WrenForeignMethodFn bindForeignMethodFn(WrenVM *v,
                                               const char *module,
                                               const char *className,
                                               bool isStatic,
                                               const char *signature) {
  (void)v;
  if (strcmp(module, "cardmaker") != 0) return NULL;
  if (strcmp(className, "cardmaker") != 0) return NULL;
  if (!isStatic) return NULL;
  if (strcmp(signature, "setStr(_,_)") == 0) return cm_setStr;
  if (strcmp(signature, "setNum(_,_)") == 0) return cm_setNum;
  if (strcmp(signature, "getStr(_)") == 0) return cm_getStr;
  if (strcmp(signature, "getNum(_)") == 0) return cm_getNum;
  if (strcmp(signature, "memFree()") == 0) return cm_memFree;
  return NULL;
}

static void errorFn(WrenVM *v, WrenErrorType type, const char *module,
                    int line, const char *message) {
  (void)v;
  if (type == WREN_ERROR_COMPILE) {
    printf("[%s line %d] Compile Error: %s\n", module, line, message);
  } else if (type == WREN_ERROR_RUNTIME) {
    printf("[line %d in %s] Runtime Error: %s\n", line, module, message);
  } else {
    printf("  [file %s line %d]\n", module, line);
  }
}

// ---------------------------
//          Init / Eval
// ---------------------------

void wren_ext_init(void) {
  if (vm != NULL) return;
  WrenConfiguration config;
  wrenInitConfiguration(&config);
  config.reallocateFn = reallocate;
  config.writeFn = writeFn;
  config.errorFn = errorFn;
  config.loadModuleFn = loadModuleFn;
  config.bindForeignMethodFn = bindForeignMethodFn;
  vm = wrenNewVM(&config);
}

void wren_ext_eval_script_file(void) {
  if (vm == NULL) wren_ext_init();

  // get script file name from string variable "script_file"
  char name[SMALL_BUF_SIZE];
  if (get_string_length("script_file") < 0 ||
      get_string("script_file", name, SMALL_BUF_SIZE) < 0) {
    printf("Error: string variable \"script_file\" is nil.\n");
    return;
  }

  // Read the script source from the vfile, then interpret it with the file
  // name as module name (so relative imports resolve against it).
  char *source = read_vfile(name);
  if (source == NULL) {
    printf("Error: failed to open script file.\n");
    return;
  }

  WrenInterpretResult res = wrenInterpret(vm, name, source);
  free(source);
  switch (res) {
    case WREN_RESULT_SUCCESS:
      break;
    case WREN_RESULT_COMPILE_ERROR:
      printf("Compile Error.\n");
      break;
    case WREN_RESULT_RUNTIME_ERROR:
      printf("Runtime Error.\n");
      break;
    default:
      break;
  }
  wrenCollectGarbage(vm);
}
