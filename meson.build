project('mimamorikanshi', 'c',
  version: '0.1.0',
  license: 'GPL-2.0-or-later',
  default_options: [
    'c_std=c11',
    'warning_level=2',
  ],
)

# Dependencies
gtk3_dep = dependency('gtk+-3.0', version: '>= 3.20')
panel_dep = dependency('libxfce4panel-2.0', version: '>= 4.14')
ui_dep = dependency('libxfce4ui-2', version: '>= 4.14')
xfconf_dep = dependency('libxfconf-0', version: '>= 4.14')
cairo_dep = dependency('cairo')
math_dep = meson.get_compiler('c').find_library('m', required: false)

# Plugin install directories
panel_pluginsdir = panel_dep.get_variable(pkgconfig: 'pluginsdir',
  default_value: join_paths(get_option('libdir'), 'xfce4', 'panel', 'plugins'))
panel_datadir = join_paths(get_option('datadir'), 'xfce4', 'panel', 'plugins')

subdir('src')
subdir('data')
