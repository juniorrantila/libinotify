inotify_polyfill_lib = library('inotify-polyfill', [
  'inotify.c'
], install: true)

install_headers('inotify.h', subdir: 'sys')

inotify_polyfill_dep = declare_dependency(
  link_with: inotify_polyfill_lib,
  include_directories: '..'
)
