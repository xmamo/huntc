# HuntC: [Hoogle](https://hoogle.haskell.org) for C

Credit to [Tsoding](https://github.com/tsoding) for this great idea.

## What is HuntC?

HuntC allows you to hunt for C functions _by signature_. This is useful for when you suppose a
function you need is available in a library you're using, but you don't quite know its name.

## How do I use HuntC?

HuntC is accessed through a command line interface. It's usage is pretty simple:

```
huntc <input_file ...> -q <query>
```

To query the standard library, you can use the `-c` flag instead of passing input files explicitly.

Say you want to convert a string to an integer. You know this function is in the standard library:
how is it called?

```
$ huntc -cq 'int(const char*)' | less
[...]
[...]/stdio.h:178:6: int puts(const char *)
[...]/stdio.h:179:6: int remove(const char *)
[...]/stdlib.h:135:6: int atoi(const char *)
[...]/stdlib.h:184:6: int system(const char *)
[...]
```

That's neat! What are all the functions which operate on a pair of `double`s?

```
$ huntc -cq 'double(double, double)' | less
[...]/math.h:321:15: extern double atan2(double, double)
[...]/math.h:425:15: extern double hypot(double, double)
[...]/math.h:429:15: extern double pow(double, double)
[...]/math.h:500:15: extern double fmod(double, double)
[...]
```

As you can see, HuntC makes exploring a library's API very convenient.

The syntax for queries reflects the syntax for function prototypes. To convert a prototype to an
appropriate query, just remove the function's name and the final semicolon; i.e.
`int atoi(const char *);` ▷ `int(const char *)`.

## Is this all?

For now, this is all there is. The command line interface is pretty trivial and there is much more
to be done. But even in this stage, HuntC is a pretty powerful tool.

[Tsoding](https://github.com/tsoding) originally came up with the idea replicating
[Hoogle](https://hoogle.haskell.org) for C; he is currently implementing his version, Coogle, which
is not yet released. As such, it would only be fair to wait for his release before considering
future developments on this version.
