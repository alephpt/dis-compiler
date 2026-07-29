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

**Writing past a `\0`**

`log` stops at the first `\0`, but `write` works from the length, so the whole
span reaches the file:

```
write -> "out.bin", "before\0after".      // all 12 bytes land
```


**As Loop** (For Loop)
```
// logic:
// as, initialization.(update expression) test condition
as, def i <- 0.(++) < 7: 
    log -> i.
```
