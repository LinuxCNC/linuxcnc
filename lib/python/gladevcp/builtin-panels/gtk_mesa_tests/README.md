# GTK Mesa tests screen
Embedded panel for PC and Mesa tests by zz912

This screen allows the user to verify whether they have a suitable and well-tuned PC for their Mesa card.

Based on the design of Mesa Configuration Tool II
https://github.com/jethornton/mesact
Copyright (c) 2022 jethornton

---

### Load with gladevcp command:
```bash
gladevcp gtk_mesa_tests
```

---

## Modification INI file for embedded panel in Gmoccapy:
```ini
EMBED_TAB_NAME = Mesa PC
EMBED_TAB_LOCATION = ntb_setup
EMBED_TAB_COMMAND = gladevcp -x {XID} gtk_mesa_tests
```


