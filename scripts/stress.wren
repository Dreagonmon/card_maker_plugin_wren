// malloc stress: churn Wren objects (small chunks), big strings
// (dlmalloc large chunks), lists (realloc-heavy), with periodic
// mass-free to force fragmentation + reuse.
// NOTE: no Fn literals (WREN_OPT_META=0); PRNG inlined.
var rnd = 12345
var alive = []
var total = 0

for (i in 0...20000) {
  rnd = (rnd * 1103515245 + 12345) % 2147483648
  var r = rnd
  if (r % 3 == 0) {
    alive.add(i)
  } else if (r % 3 == 1) {
    // medium string built by concatenation -> many varied malloc/realloc
    var n = 5 + (r % 60)
    var s = ""
    for (j in 0...n) {
      s = s + "x" + (j % 10).toString
    }
    alive.add(s)
    total = total + n * 3
  } else {
    // large string -> dlmalloc large chunk path
    var n = 2000 + (r % 8000)
    var s = "a"
    while (s.count < n) {
      s = s + s
    }
    alive.add(s)
    total = total + s.count
  }
  // free roughly half of what's alive periodically
  if (i % 500 == 499) {
    var keep = []
    for (o in alive) {
      rnd = (rnd * 1103515245 + 12345) % 2147483648
      if (rnd % 2 == 0) {
        keep.add(o)
      }
    }
    alive = keep
  }
}
System.print("stress ok, alive=%(alive.count) bytes~%(total)")
