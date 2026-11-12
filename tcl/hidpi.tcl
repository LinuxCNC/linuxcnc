# HiDPI support for LinuxCNC's Tk programs.
#
# Tk 8.6 has no notion of a desktop scale factor.  On a HiDPI desktop the
# X server (or XWayland with native scaling) reports an ordinary 96 dpi screen
# and announces the real scale only through the Xft.dpi resource, e.g. 192
# for 200%.  Text already follows Xft.dpi, because Tk's Xft font code applies
# it, but everything Tk measures in pixels does not: widget defaults such as
# slider and scrollbar sizes, fixed window sizes and images all come out at
# their 96 dpi size.
#
# Source this file right after Tk is initialised and call
#     linuxcnc::hidpi::init
# Then use [linuxcnc::hidpi::scale] for pixel sizes the program sets itself,
# and linuxcnc::hidpi::scale_image for images.
#
# The scale can be forced with the LINUXCNC_UI_SCALE environment variable
# (e.g. LINUXCNC_UI_SCALE=1.5); LINUXCNC_UI_SCALE=1 disables scaling.

namespace eval linuxcnc::hidpi {
    variable factor 1.0
    variable initialised 0
}

# Desktop scale relative to 96 dpi, rounded to a quarter step.
proc linuxcnc::hidpi::detect {} {
    if {[info exists ::env(LINUXCNC_UI_SCALE)]
            && [string is double -strict $::env(LINUXCNC_UI_SCALE)]
            && $::env(LINUXCNC_UI_SCALE) > 0} {
        return [expr {double($::env(LINUXCNC_UI_SCALE))}]
    }
    set dpi 96.0
    if {![catch {exec xrdb -query} res]} {
        foreach line [split $res \n] {
            if {[regexp {^Xft\.dpi:\s*([0-9.]+)} $line -> d] && $d > 0} {
                set dpi $d
            }
        }
    }
    set f [expr {round($dpi / 96.0 * 4) / 4.0}]
    if {$f < 1.0} { set f 1.0 }
    return $f
}

proc linuxcnc::hidpi::init {} {
    variable factor
    variable initialised
    if {$initialised} { return $factor }
    set initialised 1
    set factor [detect]

    # Sizes given in screen units (points, mm, inches) - many Tk defaults,
    # e.g. button padding - follow tk scaling.
    tk scaling [expr {$factor * 96.0 / 72.0}]

    if {$factor == 1.0} { return $factor }

    # Pixel defaults of the classic widgets.  startupFile priority: above
    # widgetDefault, which BWidget uses for its own pixel defaults when it is
    # loaded later, but below ~/.Xresources (userDefault) and a program's own
    # "option add" (interactive) or explicit widget options.
    # Only options whose built-in default is non-zero are listed, per class:
    # a blanket *borderWidth would also give frames and labels a border.
    foreach {pattern px} {
        *Scale.width              15
        *Scale.sliderLength       30
        *Scale.length            100
        *Scrollbar.width          11
        *Scrollbar.elementBorderWidth 1
        *selectBorderWidth         1
        *activeBorderWidth         1
        *insertWidth               2
        *Button.borderWidth        2
        *Checkbutton.borderWidth   2
        *Radiobutton.borderWidth   2
        *Menubutton.borderWidth    2
        *Scale.borderWidth         2
        *Scrollbar.borderWidth     1
        *Entry.borderWidth         1
        *Spinbox.borderWidth       1
        *Listbox.borderWidth       1
        *Text.borderWidth          1
        *Menu.borderWidth          1
        *Button.highlightThickness       1
        *Checkbutton.highlightThickness  1
        *Radiobutton.highlightThickness  1
        *Scale.highlightThickness        1
        *Entry.highlightThickness        1
        *Spinbox.highlightThickness      1
        *Listbox.highlightThickness      1
        *Text.highlightThickness         1
        *Tree.deltax              10
        *Tree.padX                20
    } {
        option add $pattern [scale $px] startupFile
    }

    # ttk widgets size their arrows, sliders and indicators in pixels.
    catch {
        foreach theme [ttk::style theme names] {
            ttk::style theme settings $theme {
                ttk::style configure . -arrowsize [scale 12] \
                    -sliderthickness [scale 15] -sliderlength [scale 30] \
                    -indicatordiameter [scale 12] -indicatorsize [scale 12] \
                    -gripcount [scale 5]
            }
        }
    }
    return $factor
}

# The current scale factor (1.0 when unscaled).
proc linuxcnc::hidpi::factor {} {
    variable factor
    return $factor
}

# Scale a pixel size.
proc linuxcnc::hidpi::scale {px} {
    variable factor
    return [expr {int(round($px * $factor))}]
}

# Scale a photo image in place to the current factor.  Integer factors are
# exact pixel doubling; others are approximated as zoom/subsample in halves
# (e.g. 1.5 = zoom 3, subsample 2).
proc linuxcnc::hidpi::scale_image {img} {
    variable factor
    if {$factor == 1.0 || [image type $img] ne "photo"} { return $img }
    set zoom [expr {int(round($factor * 2))}]
    set sub 2
    if {$zoom % 2 == 0} { set zoom [expr {$zoom / 2}]; set sub 1 }
    set tmp [image create photo]
    $tmp copy $img
    $img blank
    $img configure -width 0 -height 0
    if {$sub == 1} {
        $img copy $tmp -zoom $zoom
    } else {
        set big [image create photo]
        $big copy $tmp -zoom $zoom
        $img copy $big -subsample $sub
        image delete $big
    }
    image delete $tmp
    return $img
}
