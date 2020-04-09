[DISPLAY]

EMBED_TAB_NAME= moveoff gladevcp demo
EMBED_TAB_COMMAND= halcmd loadusr -Wn gladevcp8 gladevcp -c gladevcp8 -t Redmond -x {XID} ./moveoff_gladevcp_8.ui
EMBED_TAB_LOCATION = vcp_box
# halcmd options used:
#       -Wn NAME   wait for named component (must agree with names in moveoff_display_8.hal)
# gladevcp options:
#       -c NAME   set component name
#       -t THEME  set theme name (should be same as desktop appearance theme usually)
#       -x {XID}  reparent in window

# to test with gscreen, change 'DISPLAY = axis' to 'DISPLAY = gscreen'
                DISPLAY = axis
             CYCLE_TIME = 0.100
              HELP_FILE = doc/help.txt
        POSITION_OFFSET = RELATIVE
      POSITION_FEEDBACK = ACTUAL
      MAX_FEED_OVERRIDE = 1.2
   MAX_SPINDLE_OVERRIDE = 1.0
    MAX_LINEAR_VELOCITY = 5
DEFAULT_LINEAR_VELOCITY = .25
  DEFAULT_SPINDLE_SPEED = 200
         PROGRAM_PREFIX = ../../nc_files/
          INTRO_GRAPHIC = linuxcnc.gif
             INTRO_TIME = 5
                 #EDITOR = gedit
            TOOL_EDITOR = tooledit
             INCREMENTS = 1 in, 0.1 in, 10 mil, 1 mil, 1mm, .1mm, 1/8000 in

