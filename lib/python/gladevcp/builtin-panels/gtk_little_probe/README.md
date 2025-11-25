# Modification of Probe Screen application by Serguei Glavatski (c) 2015
This modification moved the icons from one window to containers with switchable vertical tabs. The goal of this modification is to minimize the window.
The modification was made by zz912.


**Actual version and manual:**
- [https://vers.ge/en/blog/useful-articles/probe-screen-v28](https://vers.ge/en/blog/useful-articles/probe-screen-v28)
- [https://github.com/verser-git/probe_screen_v2.9](https://github.com/verser-git/probe_screen_v2.9)

---

### Load with gladevcp command:
```bash
gladevcp gtk_little_probe
```

---

## Modification INI file for embedded panel in Gmoccapy:
```ini
[DISPLAY]
DISPLAY = gmoccapy
EMBED_TAB_NAME = Probe
EMBED_TAB_LOCATION = ntb_preview
EMBED_TAB_COMMAND = gladevcp -x {XID} gtk_little_probe

[TOOLSENSOR]
RAPID_SPEED = 600

[RS274NGC]
# for package install:
SUBROUTINE_PATH = ./macros:/usr/share/linuxcnc/nc_files/probe/gtk_probe/

# For RIP installation, use the path according to your directory:
# SUBROUTINE_PATH = ./macros:~/linuxcnc/nc_files/probe/gtk_probe/
```
Use only one Probe Screen by Serguei Glavatski in one LCNC configuration.


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
