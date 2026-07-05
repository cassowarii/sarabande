# Sarabande

This is a little dynamically-typed programming language I've been working on.

It is still extremely a work in progress, but we can already do some little basic test math programs:

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
