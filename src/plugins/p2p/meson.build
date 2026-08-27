# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_p2p_deps = [
  libvalent_dep,
]

# Sources
plugin_p2p_sources = files([
  'p2p-plugin.c',
  'valent-p2p-plugin.c',
])

plugin_p2p_include_directories = [include_directories('.')]

# Resources
plugin_p2p_info = i18n.merge_file(
    args: plugins_po_args,
   input: configure_file(
            input: files('p2p.plugin.desktop.in'),
           output: 'p2p.plugin.desktop',
    configuration: plugins_conf,
  ),
  output: 'p2p.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_p2p_resources = gnome.compile_resources('p2p-resources',
                                                'p2p.gresource.xml',
        c_name: 'p2p',
  dependencies: [plugin_p2p_info],
)
plugin_p2p_sources += plugin_p2p_resources

# Static Build
plugin_p2p = static_library('plugin-p2p',
                             plugin_p2p_sources,
    include_directories: plugin_p2p_include_directories,
           dependencies: plugin_p2p_deps,
                 c_args: plugins_c_args + release_args,
  gnu_symbol_visibility: 'hidden',
)

plugins_static += [plugin_p2p]
