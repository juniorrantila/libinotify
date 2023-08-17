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
