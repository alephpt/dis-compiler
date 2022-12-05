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


**As Loop** (For Loop)
```
// logic:
// as, initialization.(update expression) test condition
as, def i <- 0.(++) < 7: 
    log -> i.
```
