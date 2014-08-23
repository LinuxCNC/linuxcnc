# Copyright (c) 1998-2002, Bryan Oakley
# All Rights Reservered
#
# Bryan Oakley
# oakley@bardo.clearlight.com
#
# combobox v2.2 September 22, 2002
#
# a combobox / dropdown listbox (pick your favorite name) widget 
# written in pure tcl
#
# this code is freely distributable without restriction, but is 
# provided as-is with no warranty expressed or implied. 
#
# thanks to the following people who provided beta test support or
# patches to the code (in no particular order):
#
# Scott Beasley     Alexandre Ferrieux      Todd Helfter
# Matt Gushee       Laurent Duperval        John Jackson
# Fred Rapp         Christopher Nelson
# Eric Galluzzo     Jean-Francois Moine
#
# A special thanks to Martin M. Hunt who provided several good ideas, 
# and always with a patch to implement them. Jean-Francois Moine, 
# Todd Helfter and John Jackson were also kind enough to send in some 
# code patches.
#
# ... and many others over the years.

package require Tk 8.0
package provide combobox 2.2

namespace eval ::combobox {

    # this is the public interface
    namespace export combobox

    # these contain references to available options
    variable widgetOptions

    # these contain references to available commands and subcommands
    variable widgetCommands
    variable scanCommands
    variable listCommands
}

# ::combobox::combobox --
#
#     This is the command that gets exported. It creates a new
#     combobox widget.
#
# Arguments:
#
#     w        path of new widget to create
#     args     additional option/value pairs (eg: -background white, etc.)
#
# Results:
#
#     It creates the widget and sets up all of the default bindings
#
# Returns:
#
#     The name of the newly create widget

proc ::combobox::combobox {w args} {
    variable widgetOptions
    variable widgetCommands
    variable scanCommands
    variable listCommands

    # perform a one time initialization
    if {![info exists widgetOptions]} {
	Init
    }

    # build it...
    eval Build $w $args

    # set some bindings...
    SetBindings $w

    # and we are done!
    return $w
}


# ::combobox::Init --
#
#     Initialize the namespace variables. This should only be called
#     once, immediately prior to creating the first instance of the
#     widget
#
# Arguments:
#
#    none
#
# Results:
#
#     All state variables are set to their default values; all of 
#     the option database entries will exist.
#
# Returns:
# 
#     empty string

proc ::combobox::Init {} {
    variable widgetOptions
    variable widgetCommands
    variable scanCommands
    variable listCommands
    variable defaultEntryCursor

    array set widgetOptions [list \
	    -background          {background          Background} \
	    -bd                  -borderwidth \
	    -bg                  -background \
	    -borderwidth         {borderWidth         BorderWidth} \
	    -command             {command             Command} \
	    -commandstate        {commandState        State} \
	    -cursor              {cursor              Cursor} \
	    -disabledbackground  {disabledBackground  DisabledBackground} \
	    -disabledforeground  {disabledForeground  DisabledForeground} \
            -dropdownwidth       {dropdownWidth       DropdownWidth} \
	    -editable            {editable            Editable} \
	    -fg                  -foreground \
	    -font                {font                Font} \
	    -foreground          {foreground          Foreground} \
	    -height              {height              Height} \
	    -highlightbackground {highlightBackground HighlightBackground} \
	    -highlightcolor      {highlightColor      HighlightColor} \
	    -highlightthickness  {highlightThickness  HighlightThickness} \
	    -image               {image               Image} \
	    -maxheight           {maxHeight           Height} \
	    -opencommand         {opencommand         Command} \
	    -relief              {relief              Relief} \
	    -selectbackground    {selectBackground    Foreground} \
	    -selectborderwidth   {selectBorderWidth   BorderWidth} \
	    -selectforeground    {selectForeground    Background} \
	    -state               {state               State} \
	    -takefocus           {takeFocus           TakeFocus} \
	    -textvariable        {textVariable        Variable} \
	    -value               {value               Value} \
	    -width               {width               Width} \
	    -xscrollcommand      {xScrollCommand      ScrollCommand} \
    ]


    set widgetCommands [list \
	    bbox      cget     configure    curselection \
	    delete    get      icursor      index        \
	    insert    list     scan         selection    \
	    xview     select   toggle       open         \
            close     \
    ]

    set listCommands [list \
	    delete       get      \
            index        insert       size \
    ]

    set scanCommands [list mark dragto]

    # why check for the Tk package? This lets us be sourced into 
    # an interpreter that doesn't have Tk loaded, such as the slave
    # interpreter used by pkg_mkIndex. In theory it should have no
    # side effects when run 
    if {[lsearch -exact [package names] "Tk"] != -1} {

	##################################################################
	#- this initializes the option database. Kinda gross, but it works
	#- (I think). 
	##################################################################

	# the image used for the button...
	if {1} {
       	    image create bitmap ::combobox::bimage -data  {
		#define down_arrow_width 15
		#define down_arrow_height 14
		static char down_arrow_bits[] = {
		    0x00,0x80,0x00,0x80,0x00,0x80,0x00,0x80,
		    0x00,0x80,0xf8,0x8f,0xf0,0x87,0xe0,0x83,
		    0xc0,0x81,0x80,0x80,0x00,0x80,0x00,0x80,
		    0x00,0x80,0x00,0x80
		}
	    }
	 } elseif {$::tcl_platform(platform) == "windows"} {
	    image create bitmap ::combobox::bimage -data {
		#define down_arrow_width 12
		#define down_arrow_height 12
		static char down_arrow_bits[] = {
		    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		    0xfc,0xf1,0xf8,0xf0,0x70,0xf0,0x20,0xf0,
		    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00;
		}
	    }
	} else {
	    image create bitmap ::combobox::bimage -data  {
		#define down_arrow_width 15
		#define down_arrow_height 15
		static char down_arrow_bits[] = {
		    0x00,0x80,0x00,0x80,0x00,0x80,0x00,0x80,
		    0x00,0x80,0xf8,0x8f,0xf0,0x87,0xe0,0x83,
		    0xc0,0x81,0x80,0x80,0x00,0x80,0x00,0x80,
		    0x00,0x80,0x00,0x80,0x00,0x80
		}
	    }
	}

	# compute a widget name we can use to create a temporary widget
	set tmpWidget ".__tmp__"
	set count 0
	while {[winfo exists $tmpWidget] == 1} {
	    set tmpWidget ".__tmp__$count"
	    incr count
	}

	# get the scrollbar width. Because we try to be clever and draw our
	# own button instead of using a tk widget, we need to know what size
	# button to create. This little hack tells us the width of a scroll
	# bar.
	#
	# NB: we need to be sure and pick a window  that doesn't already
	# exist... 
	scrollbar $tmpWidget
	set sb_width [winfo reqwidth $tmpWidget]
	destroy $tmpWidget

	# steal options from the entry widget
	# we want darn near all options, so we'll go ahead and do
	# them all. No harm done in adding the one or two that we
	# don't use.
	entry $tmpWidget 
	foreach foo [$tmpWidget configure] {
	    # the cursor option is special, so we'll save it in
	    # a special way
	    if {[lindex $foo 0] == "-cursor"} {
		set defaultEntryCursor [lindex $foo 4]
	    }
	    if {[llength $foo] == 5} {
		set option [lindex $foo 1]
		set value [lindex $foo 4]
		option add *Combobox.$option $value widgetDefault

		# these options also apply to the dropdown listbox
		if {[string compare $option "foreground"] == 0 \
			|| [string compare $option "background"] == 0 \
			|| [string compare $option "font"] == 0} {
		    option add *Combobox*ComboboxListbox.$option $value \
			    widgetDefault
		}
	    }
	}
	destroy $tmpWidget

	# these are unique to us...
	option add *Combobox.dropdownWidth       {}     widgetDefault
	option add *Combobox.openCommand         {}     widgetDefault
	option add *Combobox.cursor              {}     widgetDefault
	option add *Combobox.commandState        normal widgetDefault
	option add *Combobox.editable            1      widgetDefault
	option add *Combobox.maxHeight           10     widgetDefault
	option add *Combobox.height              0
    }

    # set class bindings
    SetClassBindings
}

# ::combobox::SetClassBindings --
#
#    Sets up the default bindings for the widget class
#
#    this proc exists since it's The Right Thing To Do, but
#    I haven't had the time to figure out how to do all the
#    binding stuff on a class level. The main problem is that
#    the entry widget must have focus for the insertion cursor
#    to be visible. So, I either have to have the entry widget
#    have the Combobox bindtag, or do some fancy juggling of
#    events or some such. What a pain.
#
# Arguments:
#
#    none
#
# Returns:
#
#    empty string

proc ::combobox::SetClassBindings {} {

    # make sure we clean up after ourselves...
    bind Combobox <Destroy> [list ::combobox::DestroyHandler %W]

    # this will (hopefully) close (and lose the grab on) the
    # listbox if the user clicks anywhere outside of it. Note
    # that on Windows, you can click on some other app and
    # the listbox will still be there, because tcl won't see
    # that button click
    set this {[::combobox::convert %W -W]}
    bind Combobox <Any-ButtonPress>   "$this close"
    bind Combobox <Any-ButtonRelease> "$this close"

    # this helps (but doesn't fully solve) focus issues. The general
    # idea is, whenever the frame gets focus it gets passed on to
    # the entry widget
    bind Combobox <FocusIn> {::combobox::tkTabToWindow [::combobox::convert %W -W].entry}

    # this closes the listbox if we get hidden
    bind Combobox <Unmap> {[::combobox::convert %W -W] close}

    return ""
}

# ::combobox::SetBindings --
#
#    here's where we do most of the binding foo. I think there's probably
#    a few bindings I ought to add that I just haven't thought
#    about...
#
#    I'm not convinced these are the proper bindings. Ideally all
#    bindings should be on "Combobox", but because of my juggling of
#    bindtags I'm not convinced thats what I want to do. But, it all
#    seems to work, its just not as robust as it could be.
#
# Arguments:
#
#    w    widget pathname
#
# Returns:
#
#    empty string

proc ::combobox::SetBindings {w} {
    upvar ::combobox::${w}::widgets  widgets
    upvar ::combobox::${w}::options  options

    # juggle the bindtags. The basic idea here is to associate the
    # widget name with the entry widget, so if a user does a bind
    # on the combobox it will get handled properly since it is
    # the entry widget that has keyboard focus.
    bindtags $widgets(entry) \
	    [concat $widgets(this) [bindtags $widgets(entry)]]

    bindtags $widgets(button) \
	    [concat $widgets(this) [bindtags $widgets(button)]]

    # override the default bindings for tab and shift-tab. The
    # focus procs take a widget as their only parameter and we
    # want to make sure the right window gets used (for shift-
    # tab we want it to appear as if the event was generated
    # on the frame rather than the entry. 
    bind $widgets(entry) <Tab> \
	    "::combobox::tkTabToWindow \[tk_focusNext $widgets(entry)\]; break"
    bind $widgets(entry) <Shift-Tab> \
	    "::combobox::tkTabToWindow \[tk_focusPrev $widgets(this)\]; break"
    
    # this makes our "button" (which is actually a label)
    # do the right thing
    bind $widgets(button) <ButtonPress-1> [list $widgets(this) toggle]

    # this lets the autoscan of the listbox work, even if they
    # move the cursor over the entry widget.
    bind $widgets(entry) <B1-Enter> "break"

    bind $widgets(listbox) <ButtonRelease-1> \
        "::combobox::Select [list $widgets(this)] \
         \[$widgets(listbox) nearest %y\]; break"

    bind $widgets(vsb) <ButtonPress-1>   {continue}
    bind $widgets(vsb) <ButtonRelease-1> {continue}

    bind $widgets(listbox) <Any-Motion> {
	%W selection clear 0 end
	%W activate @%x,%y
	%W selection anchor @%x,%y
	%W selection set @%x,%y @%x,%y
	# need to do a yview if the cursor goes off the top
	# or bottom of the window... (or do we?)
    }

    # these events need to be passed from the entry widget
    # to the listbox, or otherwise need some sort of special
    # handling. 
    foreach event [list <Up> <Down> <Tab> <Return> <Escape> \
	    <Next> <Prior> <Double-1> <1> <Any-KeyPress> \
	    <FocusIn> <FocusOut> <KeyRelease-Up> <KeyRelease-Down> \
	    <KeyRelease-Next> <KeyRelease-Prior>] {
	bind $widgets(entry) $event \
            [list ::combobox::HandleEvent $widgets(this) $event]
    }

    # like the other events, <MouseWheel> needs to be passed from
    # the entry widget to the listbox. However, in this case we
    # need to add an additional parameter
    bind $widgets(entry) <MouseWheel> \
        [list ::combobox::HandleEvent $widgets(this) <MouseWheel> %D]
}

# ::combobox::Build --
#
#    This does all of the work necessary to create the basic
#    combobox. 
#
# Arguments:
#
#    w        widget name
#    args     additional option/value pairs
#
# Results:
#
#    Creates a new widget with the given name. Also creates a new
#    namespace patterened after the widget name, as a child namespace
#    to ::combobox
#
# Returns:
#
#    the name of the widget

proc ::combobox::Build {w args } {
    variable widgetOptions

    if {[winfo exists $w]} {
	error "window name \"$w\" already exists"
    }

    # create the namespace for this instance, and define a few
    # variables
    namespace eval ::combobox::$w {

	variable ignoreTrace 0
	variable oldFocus    {}
	variable oldGrab     {}
	variable oldValue    {}
	variable options
	variable this
	variable widgets

	set widgets(foo) foo  ;# coerce into an array
	set options(foo) foo  ;# coerce into an array

	unset widgets(foo)
	unset options(foo)
    }

    # import the widgets and options arrays into this proc so
    # we don't have to use fully qualified names, which is a
    # pain.
    upvar ::combobox::${w}::widgets widgets
    upvar ::combobox::${w}::options options

    # this is our widget -- a frame of class Combobox. Naturally,
    # it will contain other widgets. We create it here because
    # we need it in order to set some default options.
    set widgets(this)   [frame  $w -class Combobox -takefocus 0 -width 0]
    set widgets(entry)  [entry  $w.entry -takefocus 1]
    set widgets(button) [label  $w.button -takefocus 0] 

    # this defines all of the default options. We get the
    # values from the option database. Note that if an array
    # value is a list of length one it is an alias to another
    # option, so we just ignore it
    foreach name [array names widgetOptions] {
	if {[llength $widgetOptions($name)] == 1} continue

	set optName  [lindex $widgetOptions($name) 0]
	set optClass [lindex $widgetOptions($name) 1]

	set value [option get $w $optName $optClass]
	set options($name) $value
    }

    # a couple options aren't available in earlier versions of
    # tcl, so we'll set them to sane values. For that matter, if
    # they exist but are empty, set them to sane values.
    if {[string length $options(-disabledforeground)] == 0} {
        set options(-disabledforeground) $options(-foreground)
    }
    if {[string length $options(-disabledbackground)] == 0} {
        set options(-disabledbackground) $options(-background)
    }

    # if -value is set to null, we'll remove it from our
    # local array. The assumption is, if the user sets it from
    # the option database, they will set it to something other
    # than null (since it's impossible to determine the difference
    # between a null value and no value at all).
    if {[info exists options(-value)] \
	    && [string length $options(-value)] == 0} {
	unset options(-value)
    }

    # we will later rename the frame's widget proc to be our
    # own custom widget proc. We need to keep track of this
    # new name, so we'll define and store it here...
    set widgets(frame) ::combobox::${w}::$w

    # gotta do this sooner or later. Might as well do it now
    pack $widgets(entry)  -side left  -fill both -expand yes
    pack $widgets(button) -side right -fill y    -expand no

    # I should probably do this in a catch, but for now it's
    # good enough... What it does, obviously, is put all of
    # the option/values pairs into an array. Make them easier
    # to handle later on...
    array set options $args

    # now, the dropdown list... the same renaming nonsense
    # must go on here as well...
    set widgets(dropdown)   [toplevel  $w.top]
    set widgets(listbox) [listbox   $w.top.list]
    set widgets(vsb)     [scrollbar $w.top.vsb]

    pack $widgets(listbox) -side left -fill both -expand y

    # fine tune the widgets based on the options (and a few
    # arbitrary values...)

    # NB: we are going to use the frame to handle the relief
    # of the widget as a whole, so the entry widget will be 
    # flat. This makes the button which drops down the list
    # to appear "inside" the entry widget.

    $widgets(vsb) configure \
	    -command "$widgets(listbox) yview" \
	    -highlightthickness 0

    $widgets(button) configure \
	    -highlightthickness 0 \
	    -borderwidth 1 \
	    -relief raised \
	    -width [expr {[winfo reqwidth $widgets(vsb)] - 2}]

    $widgets(entry) configure \
	    -borderwidth 0 \
	    -relief flat \
	    -highlightthickness 0 

    $widgets(dropdown) configure \
	    -borderwidth 1 \
	    -relief sunken

    $widgets(listbox) configure \
	    -selectmode browse \
	    -background [$widgets(entry) cget -bg] \
	    -yscrollcommand "$widgets(vsb) set" \
	    -exportselection false \
	    -borderwidth 0


#    trace variable ::combobox::${w}::entryTextVariable w \
#	    [list ::combobox::EntryTrace $w]
	
    # do some window management foo on the dropdown window
    wm overrideredirect $widgets(dropdown) 1
    wm transient        $widgets(dropdown) [winfo toplevel $w]
    wm group            $widgets(dropdown) [winfo parent $w]
    wm resizable        $widgets(dropdown) 0 0
    wm withdraw         $widgets(dropdown)
    
    # this moves the original frame widget proc into our
    # namespace and gives it a handy name
    rename ::$w $widgets(frame)

    # now, create our widget proc. Obviously (?) it goes in
    # the global namespace. All combobox widgets will actually
    # share the same widget proc to cut down on the amount of
    # bloat. 
    proc ::$w {command args} \
        "eval ::combobox::WidgetProc $w \$command \$args"


    # ok, the thing exists... let's do a bit more configuration. 
    if {[catch "::combobox::Configure [list $widgets(this)] [array get options]" error]} {
	catch {destroy $w}
	error "internal error: $error"
    }

    return ""

}

# ::combobox::HandleEvent --
#
#    this proc handles events from the entry widget that we want
#    handled specially (typically, to allow navigation of the list
#    even though the focus is in the entry widget)
#
# Arguments:
#
#    w       widget pathname
#    event   a string representing the event (not necessarily an
#            actual event)
#    args    additional arguments required by particular events

proc ::combobox::HandleEvent {w event args} {
    upvar ::combobox::${w}::widgets  widgets
    upvar ::combobox::${w}::options  options
    upvar ::combobox::${w}::oldValue oldValue

    # for all of these events, if we have a special action we'll
    # do that and do a "return -code break" to keep additional 
    # bindings from firing. Otherwise we'll let the event fall
    # on through. 
    switch $event {

        "<MouseWheel>" {
	    if {[winfo ismapped $widgets(dropdown)]} {
                set D [lindex $args 0]
                # the '120' number in the following expression has
                # it's genesis in the tk bind manpage, which suggests
                # that the smallest value of %D for mousewheel events
                # will be 120. The intent is to scroll one line at a time.
                $widgets(listbox) yview scroll [expr {-($D/120)}] units
            }
        } 

	"<Any-KeyPress>" {
	    # if the widget is editable, clear the selection. 
	    # this makes it more obvious what will happen if the 
	    # user presses <Return> (and helps our code know what
	    # to do if the user presses return)
	    if {$options(-editable)} {
		$widgets(listbox) see 0
		$widgets(listbox) selection clear 0 end
		$widgets(listbox) selection anchor 0
		$widgets(listbox) activate 0
	    }
	}

	"<FocusIn>" {
	    set oldValue [$widgets(entry) get]
	}

	"<FocusOut>" {
	    if {![winfo ismapped $widgets(dropdown)]} {
		# did the value change?
		set newValue [$widgets(entry) get]
		if {$oldValue != $newValue} {
		    CallCommand $widgets(this) $newValue
		}
	    }
	}

	"<1>" {
	    set editable [::combobox::GetBoolean $options(-editable)]
	    if {!$editable} {
		if {[winfo ismapped $widgets(dropdown)]} {
		    $widgets(this) close
		    return -code break;

		} else {
		    if {$options(-state) != "disabled"} {
			$widgets(this) open
			return -code break;
		    }
		}
	    }
	}

	"<Double-1>" {
	    if {$options(-state) != "disabled"} {
		$widgets(this) toggle
		return -code break;
	    }
	}

	"<Tab>" {
	    if {[winfo ismapped $widgets(dropdown)]} {
		::combobox::Find $widgets(this) 0
		return -code break;
	    } else {
		::combobox::SetValue $widgets(this) [$widgets(this) get]
	    }
	}

	"<Escape>" {
#	    $widgets(entry) delete 0 end
#	    $widgets(entry) insert 0 $oldValue
	    if {[winfo ismapped $widgets(dropdown)]} {
		$widgets(this) close
		return -code break;
	    }
	}

	"<Return>" {
	    # did the value change?
	    set newValue [$widgets(entry) get]
	    if {$oldValue != $newValue} {
		CallCommand $widgets(this) $newValue
	    }

	    if {[winfo ismapped $widgets(dropdown)]} {
		::combobox::Select $widgets(this) \
			[$widgets(listbox) curselection]
		return -code break;
	    } 

	}

	"<Next>" {
	    $widgets(listbox) yview scroll 1 pages
	    set index [$widgets(listbox) index @0,0]
	    $widgets(listbox) see $index
	    $widgets(listbox) activate $index
	    $widgets(listbox) selection clear 0 end
	    $widgets(listbox) selection anchor $index
	    $widgets(listbox) selection set $index
	    return -code break;

	}

	"<Prior>" {
	    $widgets(listbox) yview scroll -1 pages
	    set index [$widgets(listbox) index @0,0]
	    $widgets(listbox) activate $index
	    $widgets(listbox) see $index
	    $widgets(listbox) selection clear 0 end
	    $widgets(listbox) selection anchor $index
	    $widgets(listbox) selection set $index
	    return -code break;
	}

	"<Down>" {
	    if {[winfo ismapped $widgets(dropdown)]} {
		::combobox::tkListboxUpDown $widgets(listbox) 1
	    } else {
		if {$options(-state) != "disabled"} {
		    $widgets(this) open
		}
	    }
	    return -code break;
	}
	"<Up>" {
	    if {[winfo ismapped $widgets(dropdown)]} {
		::combobox::tkListboxUpDown $widgets(listbox) -1
	    } else {
		if {$options(-state) != "disabled"} {
		    $widgets(this) open
		}
	    }
	    return -code break;
	}

	"<KeyRelease-Up>" - "<KeyRelease-Down>" -
	"<KeyRelease-Next>" - "<KeyRelease-Prior>" {
	    if {[winfo ismapped $widgets(dropdown)]} {
		return -code break;
	    }
	}
    }

    return ""
}

# ::combobox::DestroyHandler {w} --
# 
#    Cleans up after a combobox widget is destroyed
#
# Arguments:
#
#    w    widget pathname
#
# Results:
#
#    The namespace that was created for the widget is deleted,
#    and the widget proc is removed.

proc ::combobox::DestroyHandler {w} {

    # if the widget actually being destroyed is of class Combobox,
    # crush the namespace and kill the proc. Get it? Crush. Kill. 
    # Destroy. Heh. Danger Will Robinson! Oh, man! I'm so funny it
    # brings tears to my eyes.
    if {[string compare [winfo class $w] "Combobox"] == 0} {
	upvar ::combobox::${w}::widgets  widgets
	upvar ::combobox::${w}::options  options

	# delete the namespace and the proc which represents
	# our widget
	namespace delete ::combobox::$w
	rename $w {}
    }   

    return ""
}

# ::combobox::Find
#
#    finds something in the listbox that matches the pattern in the
#    entry widget and selects it
#
#    N.B. I'm not convinced this is working the way it ought to. It
#    works, but is the behavior what is expected? I've also got a gut
#    feeling that there's a better way to do this, but I'm too lazy to
#    figure it out...
#
# Arguments:
#
#    w      widget pathname
#    exact  boolean; if true an exact match is desired
#
# Returns:
#
#    Empty string

proc ::combobox::Find {w {exact 0}} {
    upvar ::combobox::${w}::widgets widgets
    upvar ::combobox::${w}::options options

    ## *sigh* this logic is rather gross and convoluted. Surely
    ## there is a more simple, straight-forward way to implement
    ## all this. As the saying goes, I lack the time to make it
    ## shorter...

    # use what is already in the entry widget as a pattern
    set pattern [$widgets(entry) get]

    if {[string length $pattern] == 0} {
	# clear the current selection
	$widgets(listbox) see 0
	$widgets(listbox) selection clear 0 end
	$widgets(listbox) selection anchor 0
	$widgets(listbox) activate 0
	return
    }

    # we're going to be searching this list...
    set list [$widgets(listbox) get 0 end]

    # if we are doing an exact match, try to find,
    # well, an exact match
    set exactMatch -1
    if {$exact} {
	set exactMatch [lsearch -exact $list $pattern]
    }

    # search for it. We'll try to be clever and not only
    # search for a match for what they typed, but a match for
    # something close to what they typed. We'll keep removing one
    # character at a time from the pattern until we find a match
    # of some sort.
    set index -1
    while {$index == -1 && [string length $pattern]} {
	set index [lsearch -glob $list "$pattern*"]
	if {$index == -1} {
	    regsub {.$} $pattern {} pattern
	}
    }

    # this is the item that most closely matches...
    set thisItem [lindex $list $index]

    # did we find a match? If so, do some additional munging...
    if {$index != -1} {

	# we need to find the part of the first item that is 
	# unique WRT the second... I know there's probably a
	# simpler way to do this... 

	set nextIndex [expr {$index + 1}]
	set nextItem [lindex $list $nextIndex]

	# we don't really need to do much if the next
	# item doesn't match our pattern...
	if {[string match $pattern* $nextItem]} {
	    # ok, the next item matches our pattern, too
	    # now the trick is to find the first character
	    # where they *don't* match...
	    set marker [string length $pattern]
	    while {$marker <= [string length $pattern]} {
		set a [string index $thisItem $marker]
		set b [string index $nextItem $marker]
		if {[string compare $a $b] == 0} {
		    append pattern $a
		    incr marker
		} else {
		    break
		}
	    }
	} else {
	    set marker [string length $pattern]
	}
	
    } else {
	set marker end
	set index 0
    }

    # ok, we know the pattern and what part is unique;
    # update the entry widget and listbox appropriately
    if {$exact && $exactMatch == -1} {
	# this means we didn't find an exact match
	$widgets(listbox) selection clear 0 end
	$widgets(listbox) see $index

    } elseif {!$exact}  {
	# this means we found something, but it isn't an exact
	# match. If we find something that *is* an exact match we
	# don't need to do the following, since it would merely 
	# be replacing the data in the entry widget with itself
	set oldstate [$widgets(entry) cget -state]
	$widgets(entry) configure -state normal
	$widgets(entry) delete 0 end
	$widgets(entry) insert end $thisItem
	$widgets(entry) selection clear
	$widgets(entry) selection range $marker end
	$widgets(listbox) activate $index
	$widgets(listbox) selection clear 0 end
	$widgets(listbox) selection anchor $index
	$widgets(listbox) selection set $index
	$widgets(listbox) see $index
	$widgets(entry) configure -state $oldstate
    }
}

# ::combobox::Select --
#
#    selects an item from the list and sets the value of the combobox
#    to that value
#
# Arguments:
#
#    w      widget pathname
#    index  listbox index of item to be selected
#
# Returns:
#
#    empty string

proc ::combobox::Select {w index} {
    upvar ::combobox::${w}::widgets widgets
    upvar ::combobox::${w}::options options
    
    if {$index != ""} {
        set data [$widgets(listbox) get [lindex $index 0]]
        ::combobox::SetValue $widgets(this) $data

        $widgets(listbox) selection clear 0 end
        $widgets(listbox) selection anchor $index
        $widgets(listbox) selection set $index

        $widgets(entry) selection range 0 end
    }

    $widgets(this) close

    return ""
}

# ::combobox::HandleScrollbar --
# 
#    causes the scrollbar of the dropdown list to appear or disappear
#    based on the contents of the dropdown listbox
#
# Arguments:
#
#    w       widget pathname
#    action  the action to perform on the scrollbar
#
# Returns:
#
#    an empty string

proc ::combobox::HandleScrollbar {w {action "unknown"}} {
    upvar ::combobox::${w}::widgets widgets
    upvar ::combobox::${w}::options options

    if {$options(-height) == 0} {
	set hlimit $options(-maxheight)
    } else {
	set hlimit $options(-height)
    }		    

    switch $action {
	"grow" {
	    if {$hlimit > 0 && [$widgets(listbox) size] > $hlimit} {
		pack $widgets(vsb) -side right -fill y -expand n
	    }
	}

	"shrink" {
	    if {$hlimit > 0 && [$widgets(listbox) size] <= $hlimit} {
		pack forget $widgets(vsb)
	    }
	}

	"crop" {
	    # this means the window was cropped and we definitely 
	    # need a scrollbar no matter what the user wants
	    pack $widgets(vsb) -side right -fill y -expand n
	}

	default {
	    if {$hlimit > 0 && [$widgets(listbox) size] > $hlimit} {
		pack $widgets(vsb) -side right -fill y -expand n
	    } else {
		pack forget $widgets(vsb)
	    }
	}
    }

    return ""
}

# ::combobox::ComputeGeometry --
#
#    computes the geometry of the dropdown list based on the size of the
#    combobox...
#
# Arguments:
#
#    w     widget pathname
#
# Returns:
#
#    the desired geometry of the listbox

proc ::combobox::ComputeGeometry {w} {
    upvar ::combobox::${w}::widgets widgets
    upvar ::combobox::${w}::options options
    
    if {$options(-height) == 0 && $options(-maxheight) != "0"} {
	# if this is the case, count the items and see if
	# it exceeds our maxheight. If so, set the listbox
	# size to maxheight...
	set nitems [$widgets(listbox) size]
	if {$nitems > $options(-maxheight)} {
	    # tweak the height of the listbox
	    $widgets(listbox) configure -height $options(-maxheight)
	} else {
	    # un-tweak the height of the listbox
	    $widgets(listbox) configure -height 0
	}
	update idletasks
    }

    # compute height and width of the dropdown list
    set bd [$widgets(dropdown) cget -borderwidth]
    set height [expr {[winfo reqheight $widgets(dropdown)] + $bd + $bd}]
    if {[string length $options(-dropdownwidth)] == 0 || 
        $options(-dropdownwidth) == 0} {
        set width [winfo width $widgets(this)]
    } else {
        set m [font measure [$widgets(listbox) cget -font] "m"]
        set width [expr {$options(-dropdownwidth) * $m}]
    }

    # figure out where to place it on the screen, trying to take into
    # account we may be running under some virtual window manager
    set screenWidth  [winfo screenwidth $widgets(this)]
    set screenHeight [winfo screenheight $widgets(this)]
    set rootx        [winfo rootx $widgets(this)]
    set rooty        [winfo rooty $widgets(this)]
    set vrootx       [winfo vrootx $widgets(this)]
    set vrooty       [winfo vrooty $widgets(this)]

    # the x coordinate is simply the rootx of our widget, adjusted for
    # the virtual window. We won't worry about whether the window will
    # be offscreen to the left or right -- we want the illusion that it
    # is part of the entry widget, so if part of the entry widget is off-
    # screen, so will the list. If you want to change the behavior,
    # simply change the if statement... (and be sure to update this
    # comment!)
    set x  [expr {$rootx + $vrootx}]
    if {0} { 
	set rightEdge [expr {$x + $width}]
	if {$rightEdge > $screenWidth} {
	    set x [expr {$screenWidth - $width}]
	}
	if {$x < 0} {set x 0}
    }

    # the y coordinate is the rooty plus vrooty offset plus 
    # the height of the static part of the widget plus 1 for a 
    # tiny bit of visual separation...
    set y [expr {$rooty + $vrooty + [winfo reqheight $widgets(this)] + 1}]
    set bottomEdge [expr {$y + $height}]

    if {$bottomEdge >= $screenHeight} {
	# ok. Fine. Pop it up above the entry widget isntead of
	# below.
	set y [expr {($rooty - $height - 1) + $vrooty}]

	if {$y < 0} {
	    # this means it extends beyond our screen. How annoying.
	    # Now we'll try to be real clever and either pop it up or
	    # down, depending on which way gives us the biggest list. 
	    # then, we'll trim the list to fit and force the use of
	    # a scrollbar

	    # (sadly, for windows users this measurement doesn't
	    # take into consideration the height of the taskbar,
	    # but don't blame me -- there isn't any way to detect
	    # it or figure out its dimensions. The same probably
	    # applies to any window manager with some magic windows
	    # glued to the top or bottom of the screen)

	    if {$rooty > [expr {$screenHeight / 2}]} {
		# we are in the lower half of the screen -- 
		# pop it up. Y is zero; that parts easy. The height
		# is simply the y coordinate of our widget, minus
		# a pixel for some visual separation. The y coordinate
		# will be the topof the screen.
		set y 1
		set height [expr {$rooty - 1 - $y}]

	    } else {
		# we are in the upper half of the screen --
		# pop it down
		set y [expr {$rooty + $vrooty + \
			[winfo reqheight $widgets(this)] + 1}]
		set height [expr {$screenHeight - $y}]

	    }

	    # force a scrollbar
	    HandleScrollbar $widgets(this) crop
	}	   
    }

    if {$y < 0} {
	# hmmm. Bummer.
	set y 0
	set height $screenheight
    }

    set geometry [format "=%dx%d+%d+%d" $width $height $x $y]

    return $geometry
}

# ::combobox::DoInternalWidgetCommand --
#
#    perform an internal widget command, then mung any error results
#    to look like it came from our megawidget. A lot of work just to
#    give the illusion that our megawidget is an atomic widget
#
# Arguments:
#
#    w           widget pathname
#    subwidget   pathname of the subwidget 
#    command     subwidget command to be executed
#    args        arguments to the command
#
# Returns:
#
#    The result of the subwidget command, or an error

proc ::combobox::DoInternalWidgetCommand {w subwidget command args} {
    upvar ::combobox::${w}::widgets widgets
    upvar ::combobox::${w}::options options

    set subcommand $command
    set command [concat $widgets($subwidget) $command $args]
    if {[catch $command result]} {
	# replace the subwidget name with the megawidget name
	regsub $widgets($subwidget) $result $widgets(this) result

	# replace specific instances of the subwidget command
	# with out megawidget command
	switch $subwidget,$subcommand {
	    listbox,index  {regsub "index"  $result "list index"  result}
	    listbox,insert {regsub "insert" $result "list insert" result}
	    listbox,delete {regsub "delete" $result "list delete" result}
	    listbox,get    {regsub "get"    $result "list get"    result}
	    listbox,size   {regsub "size"   $result "list size"   result}
	}
	error $result

    } else {
	return $result
    }
}


# ::combobox::WidgetProc --
#
#    This gets uses as the widgetproc for an combobox widget. 
#    Notice where the widget is created and you'll see that the
#    actual widget proc merely evals this proc with all of the
#    arguments intact.
#
#    Note that some widget commands are defined "inline" (ie:
#    within this proc), and some do most of their work in 
#    separate procs. This is merely because sometimes it was
#    easier to do it one way or the other.
#
# Arguments:
#
#    w         widget pathname
#    command   widget subcommand
#    args      additional arguments; varies with the subcommand
#
# Results:
#
#    Performs the requested widget command

proc ::combobox::WidgetProc {w command args} {
    upvar ::combobox::${w}::widgets widgets
    upvar ::combobox::${w}::options options
    upvar ::combobox::${w}::oldFocus oldFocus
    upvar ::combobox::${w}::oldGrab oldGrab

    set command [::combobox::Canonize $w command $command]

    # this is just shorthand notation...
    set doWidgetCommand \
	    [list ::combobox::DoInternalWidgetCommand $widgets(this)]

    if {$command == "list"} {
	# ok, the next argument is a list command; we'll 
	# rip it from args and append it to command to
	# create a unique internal command
	#
	# NB: because of the sloppy way we are doing this,
	# we'll also let the user enter our secret command
	# directly (eg: listinsert, listdelete), but we
	# won't document that fact
	set command "list-[lindex $args 0]"
	set args [lrange $args 1 end]
    }

    set result ""

    # many of these commands are just synonyms for specific
    # commands in one of the subwidgets. We'll get them out
    # of the way first, then do the custom commands.
    switch $command {
	bbox -
	delete -
	get -
	icursor -
	index -
	insert -
	scan -
	selection -
	xview {
	    set result [eval $doWidgetCommand entry $command $args]
	}
	list-get 	{set result [eval $doWidgetCommand listbox get $args]}
	list-index 	{set result [eval $doWidgetCommand listbox index $args]}
	list-size 	{set result [eval $doWidgetCommand listbox size $args]}

	select {
	    if {[llength $args] == 1} {
		set index [lindex $args 0]
		set result [Select $widgets(this) $index]
	    } else {
		error "usage: $w select index"
	    }
	}

	subwidget {
	    set knownWidgets [list button entry listbox dropdown vsb]
	    if {[llength $args] == 0} {
		return $knownWidgets
	    }

	    set name [lindex $args 0]
	    if {[lsearch $knownWidgets $name] != -1} {
		set result $widgets($name)
	    } else {
		error "unknown subwidget $name"
	    }
	}

	curselection {
	    set result [eval $doWidgetCommand listbox curselection]
	}

	list-insert {
	    eval $doWidgetCommand listbox insert $args
	    set result [HandleScrollbar $w "grow"]
	}

	list-delete {
	    eval $doWidgetCommand listbox delete $args
	    set result [HandleScrollbar $w "shrink"]
	}

	toggle {
	    # ignore this command if the widget is disabled...
	    if {$options(-state) == "disabled"} return

	    # pops down the list if it is not, hides it
	    # if it is...
	    if {[winfo ismapped $widgets(dropdown)]} {
		set result [$widgets(this) close]
	    } else {
		set result [$widgets(this) open]
	    }
	}

	open {

	    # if this is an editable combobox, the focus should
	    # be set to the entry widget
	    if {$options(-editable)} {
		focus $widgets(entry)
		$widgets(entry) select range 0 end
		$widgets(entry) icur end
	    }

	    # if we are disabled, we won't allow this to happen
	    if {$options(-state) == "disabled"} {
		return 0
	    }

	    # if there is a -opencommand, execute it now
	    if {[string length $options(-opencommand)] > 0} {
		# hmmm... should I do a catch, or just let the normal
		# error handling handle any errors? For now, the latter...
		uplevel \#0 $options(-opencommand)
	    }

	    # compute the geometry of the window to pop up, and set
	    # it, and force the window manager to take notice
	    # (even if it is not presently visible).
	    #
	    # this isn't strictly necessary if the window is already
	    # mapped, but we'll go ahead and set the geometry here
	    # since its harmless and *may* actually reset the geometry
	    # to something better in some weird case.
	    set geometry [::combobox::ComputeGeometry $widgets(this)]
	    wm geometry $widgets(dropdown) $geometry
	    update idletasks

	    # if we are already open, there's nothing else to do
	    if {[winfo ismapped $widgets(dropdown)]} {
		return 0
	    }

	    # save the widget that currently has the focus; we'll restore
	    # the focus there when we're done
	    set oldFocus [focus]

	    # ok, tweak the visual appearance of things and 
	    # make the list pop up
	    $widgets(button) configure -relief sunken
	    raise $widgets(dropdown) [winfo parent $widgets(this)]
	    wm deiconify $widgets(dropdown) 

	    # force focus to the entry widget so we can handle keypress
	    # events for traversal
	    focus -force $widgets(entry)

	    # select something by default, but only if its an
	    # exact match...
	    ::combobox::Find $widgets(this) 1

	    # save the current grab state for the display containing
	    # this widget. We'll restore it when we close the dropdown
	    # list
	    set status "none"
	    set grab [grab current $widgets(this)]
	    if {$grab != ""} {set status [grab status $grab]}
	    set oldGrab [list $grab $status]
	    unset grab status

	    # *gasp* do a global grab!!! Mom always told me not to
	    # do things like this, but sometimes a man's gotta do
	    # what a man's gotta do.
	    grab -global $widgets(this)

	    # fake the listbox into thinking it has focus. This is 
	    # necessary to get scanning initialized properly in the
	    # listbox.
	    event generate $widgets(listbox) <B1-Enter>

            # This seems to be necessary on certain window managers
            # including twm and fluxbox
            after idle raise $widgets(dropdown)

	    return 1
	}

	close {
	    # if we are already closed, don't do anything...
	    if {![winfo ismapped $widgets(dropdown)]} {
		return 0
	    }

	    # restore the focus and grab, but ignore any errors...
	    # we're going to be paranoid and release the grab before
	    # trying to set any other grab because we really really
	    # really want to make sure the grab is released.
	    catch {focus $oldFocus} result
	    catch {grab release $widgets(this)}
	    catch {
		set status [lindex $oldGrab 1]
		if {$status == "global"} {
		    grab -global [lindex $oldGrab 0]
		} elseif {$status == "local"} {
		    grab [lindex $oldGrab 0]
		}
		unset status
	    }

	    # hides the listbox
	    $widgets(button) configure -relief raised
	    wm withdraw $widgets(dropdown) 

	    # select the data in the entry widget. Not sure
	    # why, other than observation seems to suggest that's
	    # what windows widgets do.
	    set editable [::combobox::GetBoolean $options(-editable)]
	    if {$editable} {
		$widgets(entry) selection range 0 end
		$widgets(button) configure -relief raised
	    }


	    # magic tcl stuff (see tk.tcl in the distribution 
	    # lib directory)
	    ::combobox::tkCancelRepeat

	    return 1
	}

	cget {
	    if {[llength $args] != 1} {
		error "wrong # args: should be $w cget option"
	    }
	    set opt [::combobox::Canonize $w option [lindex $args 0]]

	    if {$opt == "-value"} {
		set result [$widgets(entry) get]
	    } else {
		set result $options($opt)
	    }
	}

	configure {
	    set result [eval ::combobox::Configure {$w} $args]
	}

	default {
	    error "bad option \"$command\""
	}
    }

    return $result
}

# ::combobox::Configure --
#
#    Implements the "configure" widget subcommand
#
# Arguments:
#
#    w      widget pathname
#    args   zero or more option/value pairs (or a single option)
#
# Results:
#    
#    Performs typcial "configure" type requests on the widget

proc ::combobox::Configure {w args} {
    variable widgetOptions
    variable defaultEntryCursor

    upvar ::combobox::${w}::widgets widgets
    upvar ::combobox::${w}::options options

    if {[llength $args] == 0} {
	# hmmm. User must be wanting all configuration information
	# note that if the value of an array element is of length
	# one it is an alias, which needs to be handled slightly
	# differently
	set results {}
	foreach opt [lsort [array names widgetOptions]] {
	    if {[llength $widgetOptions($opt)] == 1} {
		set alias $widgetOptions($opt)
		set optName $widgetOptions($alias)
		lappend results [list $opt $optName]
	    } else {
		set optName  [lindex $widgetOptions($opt) 0]
		set optClass [lindex $widgetOptions($opt) 1]
		set default [option get $w $optName $optClass]
		if {[info exists options($opt)]} {
		    lappend results [list $opt $optName $optClass \
			    $default $options($opt)]
		} else {
		    lappend results [list $opt $optName $optClass \
			    $default ""]
		}
	    }
	}

	return $results
    }
    
    # one argument means we are looking for configuration
    # information on a single option
    if {[llength $args] == 1} {
	set opt [::combobox::Canonize $w option [lindex $args 0]]

	set optName  [lindex $widgetOptions($opt) 0]
	set optClass [lindex $widgetOptions($opt) 1]
	set default [option get $w $optName $optClass]
	set results [list $opt $optName $optClass \
		$default $options($opt)]
	return $results
    }

    # if we have an odd number of values, bail. 
    if {[expr {[llength $args]%2}] == 1} {
	# hmmm. An odd number of elements in args
	error "value for \"[lindex $args end]\" missing"
    }
    
    # Great. An even number of options. Let's make sure they 
    # are all valid before we do anything. Note that Canonize
    # will generate an error if it finds a bogus option; otherwise
    # it returns the canonical option name
    foreach {name value} $args {
	set name [::combobox::Canonize $w option $name]
	set opts($name) $value
    }

    # process all of the configuration options
    # some (actually, most) options require us to
    # do something, like change the attributes of
    # a widget or two. Here's where we do that...
    #
    # note that the handling of disabledforeground and
    # disabledbackground is a little wonky. First, we have
    # to deal with backwards compatibility (ie: tk 8.3 and below
    # didn't have such options for the entry widget), and
    # we have to deal with the fact we might want to disable
    # the entry widget but use the normal foreground/background
    # for when the combobox is not disabled, but not editable either.

    set updateVisual 0
    foreach option [array names opts] {
	set newValue $opts($option)
	if {[info exists options($option)]} {
	    set oldValue $options($option)
	}

	switch -- $option {
	    -background {
		set updateVisual 1
		set options($option) $newValue
	    }

	    -borderwidth {
		$widgets(frame) configure -borderwidth $newValue
		set options($option) $newValue
	    }

	    -command {
		# nothing else to do...
		set options($option) $newValue
	    }

	    -commandstate {
		# do some value checking...
		if {$newValue != "normal" && $newValue != "disabled"} {
		    set options($option) $oldValue
		    set message "bad state value \"$newValue\";"
		    append message " must be normal or disabled"
		    error $message
		}
		set options($option) $newValue
	    }

	    -cursor {
		$widgets(frame) configure -cursor $newValue
		$widgets(entry) configure -cursor $newValue
		$widgets(listbox) configure -cursor $newValue
		set options($option) $newValue
	    }

	    -disabledforeground {
		set updateVisual 1
		set options($option) $newValue
	    }

	    -disabledbackground {
		set updateVisual 1
		set options($option) $newValue
	    }

            -dropdownwidth {
                set options($option) $newValue
            }

	    -editable {
		set updateVisual 1
		if {$newValue} {
		    # it's editable...
		    $widgets(entry) configure \
			    -state normal \
			    -cursor $defaultEntryCursor
		} else {
		    $widgets(entry) configure \
			    -state disabled \
			    -cursor $options(-cursor)
		}
		set options($option) $newValue
	    }

	    -font {
		$widgets(entry) configure -font $newValue
		$widgets(listbox) configure -font $newValue
		set options($option) $newValue
	    }

	    -foreground {
		set updateVisual 1
		set options($option) $newValue
	    }

	    -height {
		$widgets(listbox) configure -height $newValue
		HandleScrollbar $w
		set options($option) $newValue
	    }

	    -highlightbackground {
		$widgets(frame) configure -highlightbackground $newValue
		set options($option) $newValue
	    }

	    -highlightcolor {
		$widgets(frame) configure -highlightcolor $newValue
		set options($option) $newValue
	    }

	    -highlightthickness {
		$widgets(frame) configure -highlightthickness $newValue
		set options($option) $newValue
	    }
	    
	    -image {
		if {[string length $newValue] > 0} {
		    $widgets(button) configure -image $newValue
		} else {
		    $widgets(button) configure -image ::combobox::bimage
		}
		set options($option) $newValue
	    }

	    -maxheight {
		# ComputeGeometry may dork with the actual height
		# of the listbox, so let's undork it
		$widgets(listbox) configure -height $options(-height)
		HandleScrollbar $w
		set options($option) $newValue
	    }

	    -opencommand {
		# nothing else to do...
		set options($option) $newValue
	    }

	    -relief {
		$widgets(frame) configure -relief $newValue
		set options($option) $newValue
	    }

	    -selectbackground {
		$widgets(entry) configure -selectbackground $newValue
		$widgets(listbox) configure -selectbackground $newValue
		set options($option) $newValue
	    }

	    -selectborderwidth {
		$widgets(entry) configure -selectborderwidth $newValue
		$widgets(listbox) configure -selectborderwidth $newValue
		set options($option) $newValue
	    }

	    -selectforeground {
		$widgets(entry) configure -selectforeground $newValue
		$widgets(listbox) configure -selectforeground $newValue
		set options($option) $newValue
	    }

	    -state {
		if {$newValue == "normal"} {
		    set updateVisual 1
		    # it's enabled

		    set editable [::combobox::GetBoolean \
			    $options(-editable)]
		    if {$editable} {
			$widgets(entry) configure -state normal
			$widgets(entry) configure -takefocus 1
		    }

                    # note that $widgets(button) is actually a label,
                    # not a button. And being able to disable labels
                    # wasn't possible until tk 8.3. (makes me wonder
		    # why I chose to use a label, but that answer is
		    # lost to antiquity)
                    if {[info patchlevel] >= 8.3} {
                        $widgets(button) configure -state normal
                    }

		} elseif {$newValue == "disabled"}  {
		    set updateVisual 1
		    # it's disabled
		    $widgets(entry) configure -state disabled
		    $widgets(entry) configure -takefocus 0
                    # note that $widgets(button) is actually a label,
                    # not a button. And being able to disable labels
                    # wasn't possible until tk 8.3. (makes me wonder
		    # why I chose to use a label, but that answer is
		    # lost to antiquity)
                    if {$::tcl_version >= 8.3} {
                        $widgets(button) configure -state disabled 
                    }

		} else {
		    set options($option) $oldValue
		    set message "bad state value \"$newValue\";"
		    append message " must be normal or disabled"
		    error $message
		}

		set options($option) $newValue
	    }

	    -takefocus {
		$widgets(entry) configure -takefocus $newValue
		set options($option) $newValue
	    }

	    -textvariable {
		$widgets(entry) configure -textvariable $newValue
		set options($option) $newValue
	    }

	    -value {
		::combobox::SetValue $widgets(this) $newValue
		set options($option) $newValue
	    }

	    -width {
		$widgets(entry) configure -width $newValue
		$widgets(listbox) configure -width $newValue
		set options($option) $newValue
	    }

	    -xscrollcommand {
		$widgets(entry) configure -xscrollcommand $newValue
		set options($option) $newValue
	    }
	}	    

	if {$updateVisual} {UpdateVisualAttributes $w}
    }
}

# ::combobox::UpdateVisualAttributes --
#
# sets the visual attributes (foreground, background mostly) 
# based on the current state of the widget (normal/disabled, 
# editable/non-editable)
#
# why a proc for such a simple thing? Well, in addition to the
# various states of the widget, we also have to consider the 
# version of tk being used -- versions from 8.4 and beyond have
# the notion of disabled foreground/background options for various
# widgets. All of the permutations can get nasty, so we encapsulate
# it all in one spot.
#
# note also that we don't handle all visual attributes here; just
# the ones that depend on the state of the widget. The rest are 
# handled on a case by case basis
#
# Arguments:
#    w		widget pathname
#
# Returns:
#    empty string

proc ::combobox::UpdateVisualAttributes {w} {

    upvar ::combobox::${w}::widgets     widgets
    upvar ::combobox::${w}::options     options

    if {$options(-state) == "normal"} {

	set foreground $options(-foreground)
	set background $options(-background)
	
    } elseif {$options(-state) == "disabled"} {

	set foreground $options(-disabledforeground)
	set background $options(-disabledbackground)
    }

    $widgets(entry)   configure -foreground $foreground -background $background
    $widgets(listbox) configure -foreground $foreground -background $background
    $widgets(button)  configure -foreground $foreground 
    $widgets(frame)   configure -background $background

    # we need to set the disabled colors in case our widget is disabled. 
    # We could actually check for disabled-ness, but we also need to 
    # check whether we're enabled but not editable, in which case the 
    # entry widget is disabled but we still want the enabled colors. It's
    # easier just to set everything and be done with it.
    
    if {$::tcl_version >= 8.4} {
	$widgets(entry) configure \
	    -disabledforeground $foreground \
	    -disabledbackground $background
	$widgets(button)  configure -disabledforeground $foreground
	$widgets(listbox) configure -disabledforeground $foreground
    }
}

# ::combobox::SetValue --
#
#    sets the value of the combobox and calls the -command, 
#    if defined
#
# Arguments:
#
#    w          widget pathname
#    newValue   the new value of the combobox
#
# Returns
#
#    Empty string

proc ::combobox::SetValue {w newValue} {

    upvar ::combobox::${w}::widgets     widgets
    upvar ::combobox::${w}::options     options
    upvar ::combobox::${w}::ignoreTrace ignoreTrace
    upvar ::combobox::${w}::oldValue    oldValue

    if {[info exists options(-textvariable)] \
	    && [string length $options(-textvariable)] > 0} {
	set variable ::$options(-textvariable)
	set $variable $newValue
    } else {
	set oldstate [$widgets(entry) cget -state]
	$widgets(entry) configure -state normal
	$widgets(entry) delete 0 end
	$widgets(entry) insert 0 $newValue
	$widgets(entry) configure -state $oldstate
    }

    # set our internal textvariable; this will cause any public
    # textvariable (ie: defined by the user) to be updated as
    # well
#    set ::combobox::${w}::entryTextVariable $newValue

    # redefine our concept of the "old value". Do it before running
    # any associated command so we can be sure it happens even
    # if the command somehow fails.
    set oldValue $newValue


    # call the associated command. The proc will handle whether or 
    # not to actually call it, and with what args
    CallCommand $w $newValue

    return ""
}

# ::combobox::CallCommand --
#
#   calls the associated command, if any, appending the new
#   value to the command to be called.
#
# Arguments:
#
#    w         widget pathname
#    newValue  the new value of the combobox
#
# Returns
#
#    empty string

proc ::combobox::CallCommand {w newValue} {
    upvar ::combobox::${w}::widgets widgets
    upvar ::combobox::${w}::options options
    
    # call the associated command, if defined and -commandstate is
    # set to "normal"
    if {$options(-commandstate) == "normal" && \
	    [string length $options(-command)] > 0} {
	set args [list $widgets(this) $newValue]
	uplevel \#0 $options(-command) $args
    }
}


# ::combobox::GetBoolean --
#
#     returns the value of a (presumably) boolean string (ie: it should
#     do the right thing if the string is "yes", "no", "true", 1, etc
#
# Arguments:
#
#     value       value to be converted 
#     errorValue  a default value to be returned in case of an error
#
# Returns:
#
#     a 1 or zero, or the value of errorValue if the string isn't
#     a proper boolean value

proc ::combobox::GetBoolean {value {errorValue 1}} {
    if {[catch {expr {([string trim $value])?1:0}} res]} {
	return $errorValue
    } else {
	return $res
    }
}

# ::combobox::convert --
#
#     public routine to convert %x, %y and %W binding substitutions.
#     Given an x, y and or %W value relative to a given widget, this
#     routine will convert the values to be relative to the combobox
#     widget. For example, it could be used in a binding like this:
#
#     bind .combobox <blah> {doSomething [::combobox::convert %W -x %x]}
#
#     Note that this procedure is *not* exported, but is intended for
#     public use. It is not exported because the name could easily 
#     clash with existing commands. 
#
# Arguments:
#
#     w     a widget path; typically the actual result of a %W 
#           substitution in a binding. It should be either a
#           combobox widget or one of its subwidgets
#
#     args  should one or more of the following arguments or 
#           pairs of arguments:
#
#           -x <x>      will convert the value <x>; typically <x> will
#                       be the result of a %x substitution
#           -y <y>      will convert the value <y>; typically <y> will
#                       be the result of a %y substitution
#           -W (or -w)  will return the name of the combobox widget
#                       which is the parent of $w
#
# Returns:
#
#     a list of the requested values. For example, a single -w will
#     result in a list of one items, the name of the combobox widget.
#     Supplying "-x 10 -y 20 -W" (in any order) will return a list of
#     three values: the converted x and y values, and the name of 
#     the combobox widget.

proc ::combobox::convert {w args} {
    set result {}
    if {![winfo exists $w]} {
	error "window \"$w\" doesn't exist"
    }

    while {[llength $args] > 0} {
	set option [lindex $args 0]
	set args [lrange $args 1 end]

	switch -exact -- $option {
	    -x {
		set value [lindex $args 0]
		set args [lrange $args 1 end]
		set win $w
		while {[winfo class $win] != "Combobox"} {
		    incr value [winfo x $win]
		    set win [winfo parent $win]
		    if {$win == "."} break
		}
		lappend result $value
	    }

	    -y {
		set value [lindex $args 0]
		set args [lrange $args 1 end]
		set win $w
		while {[winfo class $win] != "Combobox"} {
		    incr value [winfo y $win]
		    set win [winfo parent $win]
		    if {$win == "."} break
		}
		lappend result $value
	    }

	    -w -
	    -W {
		set win $w
		while {[winfo class $win] != "Combobox"} {
		    set win [winfo parent $win]
		    if {$win == "."} break;
		}
		lappend result $win
	    }
	}
    }
    return $result
}

# ::combobox::Canonize --
#
#    takes a (possibly abbreviated) option or command name and either 
#    returns the canonical name or an error
#
# Arguments:
#
#    w        widget pathname
#    object   type of object to canonize; must be one of "command",
#             "option", "scan command" or "list command"
#    opt      the option (or command) to be canonized
#
# Returns:
#
#    Returns either the canonical form of an option or command,
#    or raises an error if the option or command is unknown or
#    ambiguous.

proc ::combobox::Canonize {w object opt} {
    variable widgetOptions
    variable columnOptions
    variable widgetCommands
    variable listCommands
    variable scanCommands

    switch $object {
	command {
	    if {[lsearch -exact $widgetCommands $opt] >= 0} {
		return $opt
	    }

	    # command names aren't stored in an array, and there
	    # isn't a way to get all the matches in a list, so
	    # we'll stuff the commands in a temporary array so
	    # we can use [array names]
	    set list $widgetCommands
	    foreach element $list {
		set tmp($element) ""
	    }
	    set matches [array names tmp ${opt}*]
	}

	{list command} {
	    if {[lsearch -exact $listCommands $opt] >= 0} {
		return $opt
	    }

	    # command names aren't stored in an array, and there
	    # isn't a way to get all the matches in a list, so
	    # we'll stuff the commands in a temporary array so
	    # we can use [array names]
	    set list $listCommands
	    foreach element $list {
		set tmp($element) ""
	    }
	    set matches [array names tmp ${opt}*]
	}

	{scan command} {
	    if {[lsearch -exact $scanCommands $opt] >= 0} {
		return $opt
	    }

	    # command names aren't stored in an array, and there
	    # isn't a way to get all the matches in a list, so
	    # we'll stuff the commands in a temporary array so
	    # we can use [array names]
	    set list $scanCommands
	    foreach element $list {
		set tmp($element) ""
	    }
	    set matches [array names tmp ${opt}*]
	}

	option {
	    if {[info exists widgetOptions($opt)] \
		    && [llength $widgetOptions($opt)] == 2} {
		return $opt
	    }
	    set list [array names widgetOptions]
	    set matches [array names widgetOptions ${opt}*]
	}

    }

    if {[llength $matches] == 0} {
	set choices [HumanizeList $list]
	error "unknown $object \"$opt\"; must be one of $choices"

    } elseif {[llength $matches] == 1} {
	set opt [lindex $matches 0]

	# deal with option aliases
	switch $object {
	    option {
		set opt [lindex $matches 0]
		if {[llength $widgetOptions($opt)] == 1} {
		    set opt $widgetOptions($opt)
		}
	    }
	}

	return $opt

    } else {
	set choices [HumanizeList $list]
	error "ambiguous $object \"$opt\"; must be one of $choices"
    }
}

# ::combobox::HumanizeList --
#
#    Returns a human-readable form of a list by separating items
#    by columns, but separating the last two elements with "or"
#    (eg: foo, bar or baz)
#
# Arguments:
#
#    list    a valid tcl list
#
# Results:
#
#    A string which as all of the elements joined with ", " or 
#    the word " or "

proc ::combobox::HumanizeList {list} {

    if {[llength $list] == 1} {
	return [lindex $list 0]
    } else {
	set list [lsort $list]
	set secondToLast [expr {[llength $list] -2}]
	set most [lrange $list 0 $secondToLast]
	set last [lindex $list end]

	return "[join $most {, }] or $last"
    }
}

# This is some backwards-compatibility code to handle TIP 44
# (http://purl.org/tcl/tip/44.html). For all private tk commands
# used by this widget, we'll make duplicates of the procs in the
# combobox namespace. 
#
# I'm not entirely convinced this is the right thing to do. I probably
# shouldn't even be using the private commands. Then again, maybe the
# private commands really should be public. Oh well; it works so it
# must be OK...
foreach command {TabToWindow CancelRepeat ListboxUpDown} {
    if {[llength [info commands ::combobox::tk$command]] == 1} break;

    set tmp [info commands tk$command]
    set proc ::combobox::tk$command
    if {[llength [info commands tk$command]] == 1} {
        set command [namespace which [lindex $tmp 0]]
        proc $proc {args} "uplevel $command \$args"
    } else {
        if {[llength [info commands ::tk::$command]] == 1} {
            proc $proc {args} "uplevel ::tk::$command \$args"
        }
    }
}

# end of combobox.tcl

