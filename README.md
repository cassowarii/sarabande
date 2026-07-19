# Sarabande

This is a little dynamically-typed programming language I've been working on.

It supports lists and hash tables. It has a sort of simple object system based around the concept of lexical closures,
and an execution model based on value semantics with explicit references. (will probably add these next)

It compiles down to a bytecode that's interpreted by a virtual machine.
It's not the fastest in the world, but performance is within spitting distance of the average scripting language.

It's still a work in progress, but it is starting to come together.

```
println `Hello, world!`
```

```
def factorial n {
    if n == 0 {
        1
    } else {
        n * factorial n - 1
    }
}

factorial 5
```

```
def sum ...list {
    let result = 0

    list.each => element {
        result = result + element
    }

    result
}

sum 1, 2, 3, 4, 5
```

```
def make_counter {
    let count = 0
    return {
        increment: => {
            count = count + 1
        },
        get: => {
            count
        }
    }
}

let c = make_counter()
c::get() # --> 0
c::increment()
c::increment()
c::increment()
c::get() # --> 3
```
