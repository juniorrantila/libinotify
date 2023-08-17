#pragma once

typedef unsigned int u32;

#define IN_MODIFY ((u32)0x00000002)

struct inotify_event {
   int wd;
   u32 mask;
   u32 cookie;

   u32 len;
   char name[];
};

int inotify_init(void);
int inotify_add_watch(int fd, char const* path, u32 mask);

