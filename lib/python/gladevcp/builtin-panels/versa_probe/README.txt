load with gladevcp command:

gladevcp versa_probe

to embed in gmoccapy:

[DISPLAY]
DISPLAY = gmoccapy
EMBED_TAB_NAME=Probe
EMBED_TAB_LOCATION = ntb_user_tabs
EMBED_TAB_COMMAND=gladevcp -x {XID} versa_probe


expects INI setting:

[TOOLSENSOR]
    RAPID_SPEED = 
