#include "inotify.h"
#include <assert.h>
#include <dlfcn.h>
#include <iso646.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/event.h>
#include <sys/syslimits.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

typedef uint32_t u32;

_Static_assert(sizeof(u32) == 4, "");

#define CUSTOM_FD_BIT ((u32)(1 << 30))
#define INOTIFY_MAX_EVENTS ((u32)1024)

typedef struct {
    int kq;

    u32 events_size;
    u32 events_capacity;
    struct kevent events[INOTIFY_MAX_EVENTS];
    char filenames[FILENAME_MAX][INOTIFY_MAX_EVENTS];
} Inotify;
static int do_inotify_init(Inotify* self);
static int inotify_deinit(Inotify* self);
static const Inotify inotify_invalid = { 0 };
static bool inotify_is_valid(Inotify* self) { return self->events_size != 0; }

static int inotify_init_slot(u32);
static int inotify_init_new_slot(void);
static Inotify* inotify_from_fd(int);

static const u32 inotifys_capacity = OPEN_MAX;
static Inotify inotifys[OPEN_MAX];
static u32 inotifys_size = 0;

static Inotify* inotify_from_fd(int fd)
{
    assert(fd & CUSTOM_FD_BIT);
    u32 slot = (u32)(fd & ~CUSTOM_FD_BIT);
    if (slot >= inotifys_size)
        return errno=EINVAL, NULL;
    return &inotifys[slot];
}

int inotify_init(void)
{
    for (u32 id = 0; id < inotifys_size; id++) {
        if (!inotify_is_valid(&inotifys[id])) {
            return inotify_init_slot(id);
        }
    }
    return inotify_init_new_slot();
}

static int inotify_init_slot(u32 id)
{
    if (do_inotify_init(&inotifys[id]) < 0)
        return -1;
    id |= CUSTOM_FD_BIT;
    return (int)id;
}

static int inotify_init_new_slot(void)
{
    if (inotifys_size >= inotifys_capacity) {
        errno = ENFILE;
        return -1;
    }
    u32 id = inotifys_size++;
    int fd = inotify_init_slot(id);
    if (fd < 0) {
        inotifys_size--;
        return -1;
    }
    return fd;
}

static int do_inotify_init(Inotify* self)
{
    int kq = kqueue();
    if (kq < 0)
        return -1;
    *self = (Inotify) {
        .kq = kq,
        .events = { 0 },
        .events_size = 0,
        .events_capacity = OPEN_MAX,
    };
    return 0;
}

static int inotify_deinit(Inotify* self)
{
    for (u32 i = 0; i < self->events_size; i++) {
        close((int)self->events[i].ident);
    }
    *self = inotify_invalid;
    return 0;
}


static int do_inotify_add_watch(Inotify*, char const* path, u32 mask);
int inotify_add_watch(int fd, char const* path, u32 mask)
{
    if (!(fd & CUSTOM_FD_BIT))
        return errno=EINVAL, -1;
    if (!path)
        return errno=EINVAL, -1;
    if (!mask)
        return errno=EINVAL, -1;

    Inotify* notify = inotify_from_fd(fd);
    if (!notify)
        return -1;
    return do_inotify_add_watch(notify, path, mask);
}

static int do_inotify_add_watch(Inotify* self, const char *path, u32 mask)
{
    if (self->events_size >= self->events_capacity)
        return errno=ENFILE, -1;
    u32 fflags = 0;
    if (mask & IN_MODIFY) {
        fflags |= NOTE_WRITE;
        mask &= ~IN_MODIFY;
    }
    if (mask != 0)
        return errno=EINVAL, -1;

    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;

    u32 id = self->events_size++;
    strncpy(self->filenames[id], path, PATH_MAX);
    self->events[id] = (struct kevent) {
        .ident = fd,
        .filter = EVFILT_VNODE,
        .flags = EV_ADD | EV_ONESHOT,
        .fflags = fflags,
        .data = 0,
        .udata = (void*)(uintptr_t)id,
    };
    return fd;
}

static int do_close(int fd);
static int (*og_close)(int fd) = 0;
int close(int fd)
{
    if (fd & CUSTOM_FD_BIT)
        return do_close(fd);

    if (!og_close) {
        void* handle = dlsym(RTLD_NEXT, "close");
        if (!handle) {
            return errno=ENOSYS, -1;
        }
        og_close = (int(*)(int))handle;
    }
    return og_close(fd);
}

static int do_close(int fd)
{
    assert(fd & CUSTOM_FD_BIT);
    fd &= ~CUSTOM_FD_BIT;
    return inotify_deinit(&inotifys[fd]);
}

typedef struct {
   int wd;
   u32 mask;
   u32 cookie;

   u32 len;
   char name[PATH_MAX + 1];
} internal_inotify_event;

static ssize_t do_read(int fd, void* buf, size_t size);
static ssize_t (*og_read)(int fd, void* buf, size_t size) = 0;
ssize_t read(int fd, void* buf, size_t size)
{
    if (fd & CUSTOM_FD_BIT)
        return do_read(fd, buf, size);

    if (!og_read) {
        void* handle = dlsym(RTLD_NEXT, "read");
        if (!handle) {
            return errno=ENOSYS, -1;
        }
        og_read = (ssize_t(*)(int, void*, size_t))handle;
    }
    return og_read(fd, buf, size);
}

static ssize_t do_read(int fd, void* buf, size_t size)
{
    if (buf == 0 || size < sizeof(struct inotify_event))
        return errno=EINVAL, -1;

    assert(fd & CUSTOM_FD_BIT);
    Inotify* notify = inotify_from_fd(fd);
    struct kevent event = { 0 };
    int res = kevent(notify->kq, notify->events, notify->events_size, &event, 1, 0);
    if (res < 0)
        return -1;
    if (event.flags & EV_ERROR)
        return -1;
    
    u32 id = (u32)(uintptr_t)event.udata;
    char const* filename = notify->filenames[id];
    u32 filename_size = strlen(filename);
    int wd = event.ident;
    u32 mask = IN_MODIFY; // FIXME: Parse event mask.
    u32 cookie = 0; // FIXME: Implement this.

    if (size - sizeof(struct inotify_event) > filename_size) {
        internal_inotify_event* ev = buf;
        *ev = (internal_inotify_event) {
           .wd = wd,
           .mask = mask,
           .cookie = cookie,
           .len = filename_size,
        };
        memcpy(ev->name, filename, filename_size);
        ev->name[filename_size] = 0;
        return sizeof(struct inotify_event) + filename_size + 1;
    }

    struct inotify_event* ev = buf;
    *ev = (struct inotify_event) {
       .wd = wd,
       .mask = mask,
       .cookie = cookie,
       .len = 0,
    };
    return 0;
}
