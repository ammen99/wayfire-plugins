plugins = [
    'primary-monitor-switch', 'rounded-corners', 'follow-cursor-bindings',
    'dynamic-workspace-switch']

deps = [wlroots, wayfire]

foreach plugin : plugins
  shared_module(plugin, plugin + '.cpp',
      dependencies: deps,
      install: true,
      install_dir: wayfire.get_variable(pkgconfig: 'plugindir'))
endforeach
