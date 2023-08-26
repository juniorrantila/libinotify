#pragma once
#include <stdint.h>

#define IN_MODIFY ((uint32_t)0x00000002)

struct inotify_event {
   int wd;
   uint32_t mask;
   uint32_t cookie;

   uint32_t len;
   char name[];
};

int inotify_init(void);
int inotify_add_watch(int fd, char const* path, uint32_t mask);
