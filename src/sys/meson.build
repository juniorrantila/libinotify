inotify_lib = library('libinotify', [
  'inotify.c'
], install: true)

install_headers('inotify.h', subdir: 'sys')

inotify_dep = declare_dependency(
  link_with: inotify_lib,
  include_directories: '..'
)
