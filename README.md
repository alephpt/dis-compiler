# dis compiler project in C

Following Crafting Interpreters (https://craftinginterpreters.com/)


# Idea

`A <- B` signifies B is going in to A.

`A -> B` signifies A is being applied to B.

A For loop declaration should be less redundant.

Comparitors should be able to support more complex logic in a simpler way:

    - `A < B < C` signifies A is less than B and B is less than C

    - `A, C < B` signifies A and C are less than B
    
    - `A == B != C` signifies A is equal to B but not C
      
    - `A, B != C` signifies A and B are not equal to C

    - `A == B == C` signifies A, B and C are all equal


# Syntax Design Idea

**Legend**
```
def - define or declare a variable
op  - define an operation
log - prints to stdout
$   - body open, or string interpolation
^   - body close, and return
.   - statement/declaration end
```

**Basic Functions**
```
op adder <- a, b : 
$
    return (a + b).
^

op main <- : $
    log -> adder-> 1, 3.
^
```
could also be written as 
```
op adder<-a,b:$^(a + b)
op main<-:$ log->adder->1,3. ^
```


## strings

A string is written between `"` or `'` and carries a length, not a terminator.

**Escapes**

Escapes are resolved once, at compile time, so a string holds real bytes and
nothing downstream has to know an escape was ever written:

```
\n      newline         0x0A
\r      carriage return 0x0D
\0      null            0x00
\\      backslash       0x5C
\"      quote           0x22
```

Any other escape is a compile error rather than a silent passthrough, so
`"\q"` and `"\t"` are both rejected.

A `\` at the end of a source line continues the string onto the next line and
carries the newline through:

```
def two <- "line one\
line two".              // holds "line one\nline two"
```

**A string holds its length, so `\0` is an ordinary byte**

Concatenation, comparison and `write` all work from the length, so bytes after
an embedded `\0` are kept and still compare:

```
log -> ("before\0after" == "before").         // false - the lengths differ
log -> ("a\0b" == "a\0c").                    // false - the byte after \0 counts
```

`log` prints through `%s` and therefore **stops at the first `\0`** —
`log -> "before\0after".` prints `before`. The rest of the bytes are still
there; only the display is cut short. Use `write` when the whole span matters.


## form — memory layout

A `form` is a packed memory layout. Fields sit in declaration order at explicit
widths with no padding of any kind, so the layout *is* the type — `rgba` and
`xyzw` are the same mechanism with different names and widths.

**Declaring a layout**
```
form Pixel <- $
    r <- u8.  g <- u8.  b <- u8.  a <- u8.
^

form Vec4 <- $
    x <- f32.  y <- f32.  z <- f32.  w <- f32.
^
```

Each field is `name <- width.` and the widths are:

```
u8  u16  u32  u64      unsigned integers
i8  i16  i32  i64      signed integers
f32 f64                floating point
```

A form is always bound globally, no matter what scope declares it. `Pixel` above
has a stride of 4 bytes, `Vec4` a stride of 16.

The width names are reserved words and cannot be used as field names, variable
names or parameter names — `def u8 <- 1.` and `x <- x.` inside a form body are
both errors.

A form name is an ordinary global binding and resolves like any other name, so a
local or a parameter shadows it for the length of its scope:

```
form Pixel <- $ r <- u8.  g <- u8. ^

$
    def Pixel <- 7.
    log -> Pixel.           // 7 - the local shadows the layout
^

log -> Pixel.               // <form Pixel> - the layout is intact
```

**Allocating**
```
def fb <- Pixel[64].        // 64 packed Pixels, zero initialized - 256 bytes
def v  <- Vec4.             // a single instance - a buffer of one
def big <- Pixel[640 * 480].// the size is a full expression, run at the alloc site
```

The buffer is a flat linear region of raw bytes — not boxed values — so a
640x480 `Pixel` framebuffer is 1,228,800 contiguous bytes.

The size must evaluate to a whole numeral of zero or greater; anything else is a
runtime error. A bare form name outside of a definition value is the layout
descriptor itself.

**Reading and writing members**
```
fb[3]::r <- 0xFF.           // indexed member store
v::x <- 1.5.                // an unindexed member addresses the first element
log -> fb[3]::r.            // 255
def n <- fb[3]::r + v::x.   // loads widen back into the numeral of the vm
```

Stores convert from the numeral of the vm into the field width using standard C
cast semantics — `fb[3]::a <- 300.` lands in a `u8` as `44`, and `-1` lands as
`255`. Loads widen the field back into a numeral. Index and field access
compose: `buffer[i]::field` for buffers, `name::field` for single instances.

An out of range index, an unknown field name, or a member access on anything
that is not a buffer is a clean runtime error — never undefined behaviour.

**Printing**
```
log -> Pixel.               // <form Pixel>
log -> fb.                  // <Pixel[64]>
```

**Numeral literals**

Field stores read naturally with the non decimal literals:

```
0xFF        // 255   hexadecimal
0b1010      // 10    binary
0c17        // 15    octal
```


## write — byte-stream output

`write` emits raw bytes to a file. It mirrors `log`, but takes a path first and
as many values as you like after it:

```
write -> path, value, value, ... .
```

The path and every value are ordinary expressions. All of them go out in one
open/write/close, truncating whatever was at the path before — the same as
`fopen` in `"wb"` mode.

**What a value contributes**

| Value | Bytes emitted |
|---|---|
| string | its own bytes — no NUL, no newline appended |
| buffer | its whole packed region, `count * stride` bytes, verbatim |

Anything else — a numeral, a boolean, `none`, a form descriptor, an operation —
is a runtime error, as is a path that is not a string.

**Nothing is written until everything checks out**

Every value is validated before the file is opened, so a bad argument can never
leave a truncated or half-written file behind. If the eighth of nine values is a
numeral, the file on disk is untouched.

A failure to open, write or close reports the reason from the system:

```
Write could not open '/proc/x/y' - No such file or directory.
```

**A PPM, hand built in pure dis**

Because a form buffer is a flat linear region, an image file is just a header
string and a buffer:

```
form RGB <- $
    r <- u8.  g <- u8.  b <- u8.
^

def fb <- RGB[8 * 8].
def y <- 0.

while, y < 8: $
    def x <- 0.

    while, x < 8: $
        fb[(y * 8) + x]::r <- x * 32.
        fb[(y * 8) + x]::g <- y * 32.
        fb[(y * 8) + x]::b <- 0x40.
        x <- x + 1.
    ^

    y <- y + 1.
^

write -> "gradient.ppm", "P6\n8 8\n255\n", fb.
```

That is a valid binary PPM — 11 bytes of header followed by 192 bytes of packed
RGB, which `file(1)` reports as `Netpbm image data, size = 8 x 8, rawbits,
pixmap`.

**Parenthesise a call used as a non-final value**

A `->` call gathers its own comma-separated arguments and keeps gathering to the
end of the statement, so a call that is not the last value in the list will
swallow the values after it:

```
write -> path, tag -> 1, "AFTER".       // calls tag with 1 AND "AFTER"
write -> path, (tag -> 1), "AFTER".     // right - two values
```

Usually the swallow is loud, because the call ends up with the wrong number of
arguments and fails at runtime. It is silent when the count happens to match the
operation's arity, so parenthesise any call that is not the final value.

**Writing past a `\0`**

`log` stops at the first `\0`, but `write` works from the length, so the whole
span reaches the file:

```
write -> "out.bin", "before\0after".      // all 12 bytes land
```


## as — counted loop

```
as, init.(update) comparison : statement
```

The left side of the comparison is **implied** — it is always the loop variable,
so `(++) < 7` reads as "step `i` up by one, while `i < 7`":

```
as, def i <- 0.(++) < 7:
    log -> i.
```

**init** either declares the loop variable with `def`, in which case the loop
owns it and it does not survive the loop, or names one that already exists, in
which case the loop mutates it and the value is still there afterwards:

```
def k <- 99.
as, k <- 0.(++) < 3: log -> k.
log -> k.                           // 3 - the loop wrote through to k
```

**update** is one of `++`, `--`, `+= expr`, `-= expr`, or empty:

```
as, def s <- 0.(+= 2) < 10:  log -> s.       // 0 2 4 6 8
as, def d <- 5.(--) > 0:     log -> d.       // 5 4 3 2 1
as, def t <- 9.(-= 3) > 0:   log -> t.       // 9 6 3
```

An empty update leaves the stepping to the body:

```
as, def e <- 0.() < 3: $
    log -> e.
    e <- e + 1.
^
```

**comparison** is one of `<`, `>`, `<=`, `>=`, `==`, `!=` followed by an
expression. Leaving it out runs the loop forever. The body is a single
statement, so use `$ ... ^` for more than one.

Loops nest the way you would expect:

```
as, def y <- 0.(++) < 3: $
    as, def x <- 0.(++) < 4: $
        log -> (y * 10) + x.
    ^
^
```


## % — modulo

`%` binds as tightly as `*` and `/` and carries the sign of the dividend:

```
log -> 10 % 3.          // 1
log -> 7.5 % 2.         // 1.5
log -> 2 + 10 % 4.      // 4
log -> 0 - 7 % 3.       // -1
```

`x % 0` answers NaN, the same way `x / 0` answers infinity — neither is an
error.


## natives

A handful of operations are implemented in C. They are ordinary values called
with the same `->` an operation uses:

```
log -> sqrt -> 9.               // 3
log -> sqrt -> 2.               // 1.41421
log -> floor -> 7 / 2.          // 3
log -> floor -> 0 - 1.5.        // -2
log -> sqrt.                    // <native sqrt>

op len <- x, y : $ ^(sqrt -> x*x + y*y)
log -> len -> 3, 4.             // 5
```

**Arithmetic**

| native | arity | answers |
|---|---|---|
| `sqrt` | 1 | the square root of a numeral |
| `floor` | 1 | the numeral rounded down to a whole number |
| `abs` | 1 | the numeral without its sign |
| `sin` `cos` `tan` | 1 | the trigonometric function, in radians |
| `atan2` | 2 | the angle of the point `y, x`, over the whole circle |
| `pow` | 2 | the first raised to the second — `pow -> 9, 0.5` is a square root |
| `min` `max` | 2 | the smaller, or the larger, of the two |

```
log -> pow -> 2, 10.            // 1024
log -> min -> 3, 8.             // 3
log -> abs -> 0 - 7.5.          // 7.5

// the clamp the renderer leans on
op clamp <- v, lo, hi : $
    return min -> (max -> v, lo), hi.
^
```

**Raster core**

These write pixels from C, which is the only way a 4K surface is affordable —
filling one from dis costs about 1.1 seconds, and `clear` does it in under 7ms.

| native | arity | answers |
|---|---|---|
| `clear` | 2 | fills every element of a four-byte-element buffer, answering the count |
| `hspan` | 6 | `hspan -> fb, w, y, x0, x1, rgba.` — one clamped run of one row, answering the pixels written |
| `read` | 2 | `read -> path, buf.` — fills a buffer from a file, answering the bytes read |

`clear` and `hspan` insist on a buffer whose elements are exactly four bytes
wide — a `u32` packed pixel. `hspan` clamps `x0` and `x1` into the row rather
than spilling into its neighbours, and writes nothing at all for a row outside
the surface or a run that ends before it starts:

```
form Px <- $ v <- u32. ^

def fb <- Px[640 * 480].

clear -> fb, 0xFF201814.
hspan -> fb, 640, 100, 0 - 20, 999, 0xFFFFFFFF.   // clamped to the whole row
```

`read` is the inverse of `write`. It never resizes — it fills as much of the
buffer as the file has bytes for, so the caller `grow`s first:

```
def bytes <- Byte[0].
grow -> bytes, 4096.
def got <- read -> "scene.dat", bytes.
```

**Buffers**

| native | arity | answers |
|---|---|---|
| `grow` | 2 | extends a buffer by n elements, answering the new count |
| `count` | 1 | how many elements a buffer currently holds |

A buffer allocated with `Form[n]` can be extended later, so a pool does not have
to know its final size. Growth is amortised, whatever it exposes reads back as
zero, and the elements already there keep their values across the move:

```
form Pt <- $ x <- f64.  y <- f64. ^

def pts <- Pt[0].

// the append idiom - grow by one, then write what grow just exposed
op append <- pool, px, py : $
    def n <- grow -> pool, 1.
    def at <- n - 1.

    pool[at]::x <- px.
    pool[at]::y <- py.

    return at.
^

log -> append -> pts, 1.5, 2.5.     // 0
log -> count -> pts.                // 1
```

Only the live count is addressable — an index at or past `count -> buf` is the
same out-of-bounds runtime error it would be for a fixed buffer, and `write`
emits `count` elements, never the spare capacity underneath.

A native checks its own arity and argument types, and a mismatch is a clean
runtime error.


## include

`include -> "path".` pulls another file in where it stands, as text. It is a
top-level statement — not legal inside an operation or a `$ ... ^` scope — and
it compiles to no bytecode at all.

```
include -> "math.dis".
include -> "scene/pools.dis".

log -> "everything above is available here".
```

A relative path resolves **against the file doing the including**, not the
working directory, so a library can name its own neighbours no matter where
`dis` was run from. In the REPL there is no including file, so paths resolve
against the working directory.

**A file is included at most once.** The second request for a file already
pulled in is silently skipped, which makes the diamond case do the right thing
and makes a cycle — including a file that includes you, or itself — harmless:

```
// e.dis and f.dis both include d.dis; the body of d runs once
include -> "e.dis".
include -> "f.dis".
```

Includes may nest 16 deep. A path that cannot be read is a compile error naming
the reason the system gave:

```
ERR - [main.dis: line 3]: '"missing.dis"' - No such file or directory
ERR - [main.dis: line 4]: '"somedir"' - Is a directory
```

Diagnostics carry the file a token actually came from, so an error inside an
included file names that file and its own line number, and a runtime traceback
names the file each operation was written in:

```
Operands must be of the same type.
[inc/boom.dis: line 3] in boom()
[main.dis: line 9] in script
```


## display

A window, if dis was built with sdl2. `compile.sh` looks for it with
`pkg-config` and defines `DIS_DISPLAY` when it is there. **The build succeeds
either way** — without sdl2 the window natives are simply never registered, so
naming one is an ordinary undefined variable rather than a link failure. `clock`
is always present; it has no sdl in it.

sdl itself is only started inside `display`, so a program that never opens a
window never touches the video subsystem.

| native | arity | answers |
|---|---|---|
| `display` | 3 | `display -> w, h, "title".` opens the window |
| `blit` | 3 | `blit -> fb, w, h.` copies a buffer to the window, one pixel to one pixel |
| `present` | 0 | shows what was blitted |
| `pump` | 0 | drains the event queue, answering how many events it saw |
| `quitting` | 0 | true once the window has been asked to close |
| `mousex` `mousey` | 0 | where the pointer is now |
| `button` | 1 | true while mouse button n is held |
| `clicked` | 1 | true if button n went down during the last `pump` |
| `wheel` | 0 | how far the wheel turned during the last `pump` |
| `key` | 1 | true while the named key is held, as in `key -> "Escape"` |
| `pressed` | 1 | true if the named key went down during the last `pump` |
| `shut` | 0 | closes the window |
| `clock` | 0 | milliseconds from a monotonic clock, in every build |

`blit` wants the buffer to match the window exactly — `w * h` elements, and the
same `w` and `h` the display was opened with. Four-byte elements are read as
RGBA, three-byte elements as RGB.

Everything `clicked`, `pressed` and `wheel` report belongs to the most recent
`pump` and is cleared by the next one.

**The frame loop**

```
form Px <- $ v <- u32. ^

def WIDE <- 1280.
def HIGH <- 720.
def fb <- Px[1280 * 720].

display -> WIDE, HIGH, "dis".

def running <- true.

while, running: $
    pump->.

    // a zero argument call only parses immediately before a '.', so the
    // answer goes into a name before it is tested
    def closing <- quitting->.
    def escape <- pressed -> "Escape".

    when, closing:  running <- false.
    when, escape:   running <- false.

    clear -> fb, 0xFF201814.

    def x <- mousex->.
    def y <- mousey->.

    as, def row <- 0.(++) < 8:
        hspan -> fb, WIDE, y + row, x, x + 8, 0xFFFFFFFF.

    blit -> fb, WIDE, HIGH.
    present->.
^

shut->.
```

`raster/p0_smoke.dis` is that loop with a sweeping bar and an FPS report. It
gives up after two seconds so it can be run unattended, and it runs headless
under `SDL_VIDEODRIVER=dummy`.

**A note on valgrind**

sdl2 caches the environment inside its own library destructor, whether or not a
window was ever opened, and never tears it down. That shows up as about 10KB
still reachable in 89 blocks on the sdl build — none of it ours, and zero
definitely, indirectly or possibly lost. `dis.supp` silences it:

```
valgrind --suppressions=dis.supp --leak-check=full --show-leak-kinds=all ./dis <file>
```

Built without sdl2, valgrind reports `All heap blocks were freed` with no
suppressions at all.
