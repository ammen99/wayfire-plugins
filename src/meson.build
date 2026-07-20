install_dir = get_option('prefix') / get_option('libdir') / 'wayfire'

#pms = shared_module('primary-monitor-switch', 'primary-monitor-switch.cpp',
#    dependencies: [wayfire],
#    install: true, install_dir: install_dir)
#
#rc = shared_module('rounded-corners', 'rounded-corners.cpp',
#    dependencies: [wayfire],
#    install: true, install_dir: install_dir)
#
#fcb = shared_module('follow-cursor-bindings', 'follow-cursor-bindings.cpp',
#    dependencies: [wayfire],
#    install: true, install_dir: install_dir)

wayfire_debugging = shared_module('ammen99-debugging', 'debugging.cpp',
    dependencies: [wayfire],
    install: true, install_dir: install_dir)

wayfire_debugging = shared_module('show-cursor', 'show-cursor.cpp',
    dependencies: [wayfire],
    install: true, install_dir: install_dir)

wayfire_debugging = shared_module('ammen99-ipc', 'myipc.cpp',
    dependencies: [wayfire],
    install: true, install_dir: install_dir)

logitech_workaround = shared_module('ammen99-logitech', 'workaround-logitech.cpp',
    dependencies: [wayfire],
    install: true, install_dir: install_dir)

super_to_special = shared_module('super-to-special', 'super-to-special.cpp',
    dependencies: [wayfire],
    install: true, install_dir: install_dir)

special_to_super = shared_module('special-to-super', 'special-to-super.cpp',
    dependencies: [wayfire],
    install: true, install_dir: install_dir)

#logitech_workaround = shared_module('blur-to-background', 'blur-to-background.cpp',
#    dependencies: [wayfire],
#    link_args: ['-lwayfire-blur-base'],
#    install: true, install_dir: install_dir)

tablet_mode = shared_module('tablet-mode', ['tablet-mode.cpp'],
    dependencies: [wayfire],
    install: true, install_dir: install_dir)

bench = shared_module('ammen99-bench', ['bench.cpp'],
    dependencies: [wayfire],
    install: true, install_dir: install_dir)
