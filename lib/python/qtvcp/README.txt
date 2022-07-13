at the moment to use designer one must create links from (hardy),
Wheezy uses slightly different location
/usr/lib/qt4/designer/python/ (linuxcnc plugins)

to:
RIP_folder /lib/python/qtvcp_widgets/widget_plugins/ (linuxcnc plugins)


for testing purposes there are two test files in RIP_folder /lib/python/qtvcp.
test1.ui is for hal only widgets
loadusr qtvcp test1.ui

test2.ui is for testing/interacting with a running linuxcnc session.
test2 requires the handler file, qtvcp_handler to be loaded.
loadusr qtvcp -u qtvcp_handler test2.ui
