AC_DEFUN([AC_PRUNE_DEFAULT_LDFLAGS],[

    dnl AC_PRUNE_DEFAULT_LDFLAGS(LIBNAME_LIBS)

    dnl Prune `-L/usr/lib` and e.g. `-L/usr/lib/arm-linux-gnueabihf`
    dnl from the supplied `LIBNAME_LIBS` variable.

    dnl Some lazy package configurations include these library paths,
    dnl which are already in the linker default search paths.
    dnl When linking with `gcc --sysroot=`, adding these breaks
    dnl linking against libraries in the sysroot.

    eval $1='"$(echo $'$1' | sed "s,-L/usr/lib ,,")"'
    if test -n "${host_alias}"; then
        eval $1='"$(echo $'$1' | sed "s,-L/usr/lib/${host_alias} ,,")"'
    fi

])
