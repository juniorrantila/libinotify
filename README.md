# libinotify

**libinotify** enables the linux file watch API on non-linux systems

## How does this work?

There were a few problems that needed to be solved to get this
to work.

### Keeping track of state.

Now, keeping track of state would be a trivial problem if all the UNIX
wouldn't use plain ints all willy nilly. If everything used void* instead,
we could've just `malloc`'ed some memory and returned that each time we
call `ìnotify_init`. This is a problem since a pointer is 64 bits while
an int is 32 bits.[^1]

So what do we do instead? Well, we use handles instead of pointers of course![^2] ¯\\\_(ツ)\_/¯

This still doesn't solve the problem completely though, for instance, we
need to avoid clashes between our custom file descriptors and actual file
descriptors so that `read` and `close` work.

So how do we avoid clashes with real file descriptors? Well, we set bit 30
to 1 of course!

**Ehm, okay.. but _why_ does that work?**

It just so happens that file descriptors are defined to be sequential,
meaning if we open 10 file descriptors, the last will have the int value of
9, and no sane person would ever have 2<sup>32</sup> open files at once, right?[^3]
Maybe we shouldn't exclude the insane from using our libraries though. :thinking:

We could technically use the sign bit (31) instead of bit 30 since the posix functions
usually return -1 to indicate error, but quite a few programmers have the habit of
checking for **anything** negative and attributing it to errors, so bit 30 it is.

### So, how the hell did I get `read` and `close` to work?

This was actually quite simple once we had a way to distinguish between
our file descriptors and the real file descriptors. We basically just hook
into the implementation of the functions, check for the magic bit and if
it's present, we call a custom implementation of close, if it's not
present, we call the original function instead. Calling the original
functions is done via [dlsym](https://man7.org/linux/man-pages/man3/dlsym.3.html).

The rest was just a matter of implementing inotify in terms of kqueue.

**TLDR**: Int is an index into an array + a magic bit and all functions are hooked at runtime.

## Build instructions

### Setup:

```sh

meson build

```

### Build:

```sh

ninja -C build

```

## Usage example

```c

#include <stdio.h>
#include <sys/syslimits.h>
#include <sys/inotify.h>
#include <unistd.h>

int main(int argc, char const* argv[]) {
    if (argc < 2) {
        (void)fprintf(stderr, "USAGE: %s file\n", argv[0]);
        return 1;
    }
    char const* path = argv[1];
    int fd = inotify_init();
    if (fd < 0) {
        perror("inotify_init");
        return 1;
    }
    if (inotify_add_watch(fd, path, IN_MODIFY) < 0) {
        perror("inotify_add_watch");
        return 1;
    }

    char buf[sizeof(struct inotify_event) + PATH_MAX];
    read(fd, buf, sizeof(buf));
    struct inotify_event* event = (struct inotify_event*)buf;
    printf("'%s' changed\n", event->name);

    close(fd);
}

```

[^1]: "Well, ackshyuwally". I know, but just play along for the sake of argument, smart-ass.
[^2]: A handle is an index into an array, in this case a global array. See: [handles are the better pointers](https://floooh.github.io/2018/06/17/handles-vs-pointers.html)
[^3]: Operating systems usually have a limit on open file descriptors at somewhere around 10 000, which is way below 2<sup>32</sup>.
