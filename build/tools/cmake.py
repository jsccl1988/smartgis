#!/usr/bin/python
# -*- coding: utf-8 -*-

import os
import sys


def cmake_build(
    cmake_root,
    cmake_options,
    build_dir,
    install_dir,
    target_cpu,
    is_debug,
    static_link_crt,
    toolset,
    winver,
):
    config = 'debug' if is_debug else 'release'
    options = ''
    if sys.platform == 'win32':
        options += '-A %s ' % ('Win32' if target_cpu == 'x86' else 'x64')
        crt_link_flags = 'MultiThreaded%s%s' % (
            'Debug' if is_debug else '',
            '' if static_link_crt else 'DLL',
        )
        options += '-DCMAKE_POLICY_DEFAULT_CMP0091=NEW -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_MSVC_RUNTIME_LIBRARY=%s ' % (
            crt_link_flags,)
    elif sys.platform == 'darwin':
        options += '-DCMAKE_OSX_ARCHITECTURES=%s ' % 'i386' if target_cpu == 'x86' else 'x86_64'
    else:
        address_model = '-m32' if target_cpu == 'x86' else '-m64'
        options += '-DCMAKE_C_FLAGS_INIT=%s -DCMAKE_CXX_FLAGS_INIT=%s ' % (
            address_model, address_model)
    options += '-DCMAKE_INSTALL_PREFIX=%s ' % install_dir

    if len(cmake_options) > 0:
        options += ' '.join(map(lambda item: '-D' + item,
                            cmake_options.split(',')))
    if sys.platform == 'win32':
        if len(winver) > 0:
            disable_warning = "/wd4819 "
            options += ' -DCMAKE_C_FLAGS_INIT="/D _WIN32_WINNT=%s %s" -DCMAKE_CXX_FLAGS_INIT="/D _WIN32_WINNT=%s %s"' % (
                winver, disable_warning, winver, disable_warning)
        # if len(toolset) > 0:
        #     options += ' -T %s' % toolset
    config_cmd = 'cmake %s -S %s -B %s' % (
        options,
        cmake_root,
        build_dir,
    )
    build_cmd = 'cmake --build %s --config %s' % (build_dir, config)
    install_cmd = 'cmake --install %s --config %s --prefix %s' % (
        build_dir,
        config,
        install_dir,
    )
    for cmd in (config_cmd, build_cmd, install_cmd):
        print(cmd)
        sys.stdout.flush()
        ret = os.system(cmd)
        if ret != 0:
            print('ERROR: cmake command failed (%s): %s' % (ret, cmd))
            sys.exit(1)


def main():
    print(sys.argv)

    [
        cmake_root,
        cmake_options,
        build_dir,
        install_dir,
        target_cpu,
        is_debug,
        static_link_crt,
        toolset,
        winver,
        dry_run,
    ] = sys.argv[1:]

    if(dry_run == "true"):
        return

    cmake_build(
        cmake_root,
        cmake_options.strip(),
        build_dir,
        install_dir,
        target_cpu,
        is_debug == 'true',
        static_link_crt == 'true',
        toolset.strip(),
        winver.strip(),
    )


if __name__ == '__main__':
    main()
