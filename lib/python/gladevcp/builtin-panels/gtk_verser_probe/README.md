# Probe Screen application by Serguei Glavatski

Old version from 2015. This older version has **less functionality**, but it **takes up less space on the screen**, which can be useful on smaller displays.

**Actual version and manual:**
- [https://vers.ge/en/blog/useful-articles/probe-screen-v28](https://vers.ge/en/blog/useful-articles/probe-screen-v28)
- [https://github.com/verser-git/probe_screen_v2.9](https://github.com/verser-git/probe_screen_v2.9)

---

### Load with gladevcp command:
```bash
gladevcp gtk_verser_probe
```

---

## Modification INI file for embedded panel in Gmoccapy:
```ini
[DISPLAY]
DISPLAY = gmoccapy
EMBED_TAB_NAME = Probe
EMBED_TAB_LOCATION = ntb_user_tabs
EMBED_TAB_COMMAND = gladevcp -x {XID} gtk_verser_probe

[TOOLSENSOR]
RAPID_SPEED = 600

[RS274NGC]
# for package install:
SUBROUTINE_PATH = ./macros:/usr/share/linuxcnc/nc_files/gtk_probe/

# For RIP installation, use the path according to your directory:
# SUBROUTINE_PATH = ./macros:~/linuxcnc/nc_files/probe/gtk_probe/
```

---

## Example using dbounce with Mesa card in HAL file:
```hal
# ---probe signal---
loadrt dbounce names=dbounce.probe
addf dbounce.probe   servo-thread

setp dbounce.probe.delay 5
net probe-db  dbounce.probe.in    <=  hm2_7i96.0.gpio.000.in
net probe-in  motion.probe-input  <=  dbounce.probe.out
```
