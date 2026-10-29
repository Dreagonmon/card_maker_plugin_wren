import { WASMEnv } from "./wasmenv.ts";

export async function loadWasmEnv(source: string) {
    const module = new WASMEnv();

    // File path: read bytes directly.
    // @ts-ignore
    const bytes = await Deno.readFile(source);

    // Try streaming compile first; it's faster for larger modules.
    // compileStreaming needs a Response, so wrap the bytes.
    const stream = new Blob([ bytes ]).stream();
    const response = new Response(stream, {
        headers: { 'Content-Type': 'application/wasm' },
    });
    await module.compileModuleStreaming(response);
    return module;
}

/* ------------------------------------------------------------------ */
/*  test                                                            */
/* ------------------------------------------------------------------ */
const scripts = `
import "cardmaker" for cardmaker

cardmaker.setStr("result", "你好世界")
cardmaker.setNum("free", 123)
var r = cardmaker.getStr("result")
var f = cardmaker.getNum("free")
var m = cardmaker.getStr("nope")
System.print("result = %(r)")
System.print("free = %(f)")
System.print("missing = %(m)")
cardmaker.setStr("@draw", "fillstyle:#ff0000")
cardmaker.setStr("@draw", "fillrect:10|10|80|80")
System.print("done")
`;
// @ts-ignore
if (import.meta.main) {
    const module = await loadWasmEnv("build/card_maker_plugin_wren.wasm");
    module.addEventListener("outputLine", (event) => {
        console.log(event.data.lastLine);
    });
    module.addEventListener("variableChanged", (event) => {
        console.log("> variable changed:", event.data.name, "=", event.data.value, "<");
    });
    await module.initInstance();
    const exp = module.getExportedFunctions<{ eval?: () => void }>();
    // write file
    const encoder = new TextEncoder();
    module.filesystem.writeFile("main.wren", encoder.encode(scripts));
    // set parameters
    module.setVariable("script_file", "main.wren");
    // eval
    exp.eval?.();
}
