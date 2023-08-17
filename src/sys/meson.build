inotify_polyfill_lib = library('inotify-polyfill', [
  'inotify.c'
])

inotify_polyfill_dep = declare_dependency(
  link_with: inotify_polyfill_lib,
  include_directories: '..'
)
