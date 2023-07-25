#pms = shared_module('primary-monitor-switch', 'primary-monitor-switch.cpp',
#    dependencies: [wayfire, wlroots],
#    install: true, install_dir: wayfire.get_variable(pkgconfig: 'plugindir'))
#
#rc = shared_module('rounded-corners', 'rounded-corners.cpp',
#    dependencies: [wayfire, wlroots],
#    install: true, install_dir: wayfire.get_variable(pkgconfig: 'plugindir'))
#
#fcb = shared_module('follow-cursor-bindings', 'follow-cursor-bindings.cpp',
#    dependencies: [wayfire, wlroots],
#    install: true, install_dir: wayfire.get_variable(pkgconfig: 'plugindir'))

switch_layouts = shared_module('switch-kb-layouts', 'switch-kb-layouts.cpp',
    dependencies: [wayfire, wlroots],
    install: true, install_dir: wayfire.get_variable(pkgconfig: 'plugindir'))

wayfire_debugging = shared_module('ammen99-debugging', 'debugging.cpp',
    dependencies: [wayfire, wlroots],
    install: true, install_dir: wayfire.get_variable(pkgconfig: 'plugindir'))

wayfire_debugging = shared_module('ammen99-ipc', 'myipc.cpp',
    dependencies: [wayfire, wlroots],
    install: true, install_dir: wayfire.get_variable(pkgconfig: 'plugindir'))

logitech_workaround = shared_module('ammen99-logitech', 'workaround-logitech.cpp',
    dependencies: [wayfire, wlroots],
    install: true, install_dir: wayfire.get_variable(pkgconfig: 'plugindir'))

logitech_workaround = shared_module('blur-to-background', 'blur-to-background.cpp',
    dependencies: [wayfire, wlroots],
    link_args: ['-lwayfire-blur-base'],
    install: true, install_dir: wayfire.get_variable(pkgconfig: 'plugindir'))
