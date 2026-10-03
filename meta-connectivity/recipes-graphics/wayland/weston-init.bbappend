do_install:append() {
    printf '\n[autolaunch]\npath=/usr/bin/hupi-ui\nwatch=false\n' >> ${D}${sysconfdir}/xdg/weston/weston.ini
}
