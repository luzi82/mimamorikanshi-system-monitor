plugin_sources = files(
  'plugin.c',
  'monitor.c',
  'config.c',
  'drawing.c',
  'dialog.c',
  'suspend.c',
  'parser/proc_stat_parser.c',
)

shared_library('mimamorikanshi',
  sources: plugin_sources,
  dependencies: [gtk3_dep, panel_dep, ui_dep, xfconf_dep, cairo_dep, math_dep],
  install: true,
  install_dir: panel_pluginsdir,
)
