# [wren](https://wren.io) port to wasm, for "the card maker" (WIP)

Embedded [wren](https://github.com/wren-lang/wren) VM compiled to wasm32,
following the architecture of
[card_maker_plugin_fe](https://github.com/Dreagonmon/card_maker_plugin_fe).

## Host interface (imports, module `env`)

Same as card_maker_plugin_fe — see `src/app.h`:

| Import | Description |
|---|---|
| `vfile_open(path)` / `vfile_close(vfp)` / `vfile_getc(vfp)` | Virtual filesystem |
| `set_string(name, text)` / `set_number(name, value)` | Set host variables (e.g. `@draw`) |
| `get_string_length(name)` / `get_string(name, buf, len)` / `get_number(name)` | Read host variables |

And the libc backend from `lib/env/wasmenv.h` (`wasmenv_putc`, `wasmenv_clock_ms`, ...).

## Exports

| Export | Description |
|---|---|
| `_start` | Initialize the wren VM |
| `eval` | Read string variable `script_file`, load that vfile and interpret it |

## `cardmaker` module

A built-in wren module is always available:

```wren
import "cardmaker" for cardmaker

cardmaker.setStr("@draw", "fillstyle:#ff0000")
cardmaker.setStr("@draw", "fillrect:10|10|80|80")
cardmaker.setNum("score", 100)
System.print(cardmaker.getStr("result"))   // null if not set
System.print(cardmaker.getNum("score"))
```

| Method | Arguments | Returns | Description |
|---|---|---|---|
| `cardmaker.setStr(name, value)` | strings | `null` | Set a host string variable |
| `cardmaker.setNum(name, value)` | string, num | `null` | Set a host number variable |
| `cardmaker.getStr(name)` | string | string or `null` | Get a host string variable |
| `cardmaker.getNum(name)` | string | num or `null` | Get a host number variable |

## Build

Required packages: `clang`, `llvm`, `wabt` (same as card_maker_plugin_fe).

```sh
git submodule update --init   # fetch lib/wren
make                          # build/card_maker_plugin_wren.wasm (+ .wat)
make run                      # run demo script via deno
```

## Layout

```
src/            main.c, wren_ext.{c,h} (VM glue + cardmaker module), app.h
lib/wren/       wren VM (git submodule, https://github.com/wren-lang/wren)
lib/env/        wasmenv.h — host import declarations for libc backend
lib/libc/       minimal freestanding libc (same as card_maker_plugin_fe)
scripts/        deno_cli.ts (test runner), wasmenv.ts (host env, shared)
```
