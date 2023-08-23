# HuntC: [Hoogle](https://hoogle.haskell.org) for C

Credit to [Tsoding](https://github.com/tsoding) for this great idea.

## What is HuntC?

HuntC allows you to hunt for C functions _by signature_. This is useful for when you suppose a
function you need is available in a library you're using, but you don't quite know its name.

## How do I use HuntC?

HuntC is accessed through a command line interface. It's usage is pretty simple:

```
huntc <input_file> -q <query>
```

Say you want to convert a string to an integer. You know this function is in the standard library:
how is it called?

```
$ huntc c.h -q 'int(const char*)' | less
[...]/stdlib.h:293:6 int cgetset(const char *)
[...]/stdlib.h:248:6 int unsetenv(const char *)
[...]/stdlib.h:135:6 int atoi(const char *)
[...]/stdio.h:178:6 int puts(const char *)
[...]
```

That's neat! What are all the functions which operate on a pair of `double`s?

```
$ huntc c.h -q 'double(double, double)' | less
[...]/math.h:321:15 extern double atan2(double, double)
[...]
[...]/math.h:684:15 extern double scalb(double, double)
[...]/math.h:425:15 extern double hypot(double, double)
[...]/math.h:429:15 extern double pow(double, double)
[...]/math.h:500:15 extern double fmod(double, double)
[...]/math.h:504:15 extern double remainder(double, double)
[...]
```

As you can see, HuntC makes exploring a librarys API very convenient.

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

## Which files do I use as input?

This depends greatly on what you're doing. If you're using a library, just use the library's main
header file as your input file.

If you're only interested in the standard library, the following could be useful. Create a single
header file (for instance `c.h`) which includes all the headers from the standard library. This way,
you don't need to guess which header contains the function(s) you're looking for. Such a header
would look like this:

```c
#include <assert.h>
#include <complex.h>
#include <ctype.h>
#include <errno.h>
#include <fenv.h>
#include <float.h>
#include <inttypes.h>
#include <iso646.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdalign.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <tgmath.h>
#include <threads.h>
#include <time.h>
#include <uchar.h>
#include <wchar.h>
#include <wctype.h>
```
