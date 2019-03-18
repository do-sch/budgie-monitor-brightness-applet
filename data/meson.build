install_data(
	'BudgieMonitorBrightnessApplet.plugin',
	install_dir: lib_install_dir
)

if get_option('set_udev_configuration')

    udev_dep = dependency('udev')
    udev_rules_dir = udev_dep.get_pkgconfig_variable('udevdir') + '/rules.d'

    install_data(
        '99-i2c_brightness_applet.rules',
        install_dir: udev_rules_dir
    )

endif

if get_option('set_kernel_module_configuration')

    cmdline_dir = '/usr/lib/modules-load.d'

    install_data(
        'i2c-dev-brightnes-applet.conf',
        install_dir: cmdline_dir
    )

endif
