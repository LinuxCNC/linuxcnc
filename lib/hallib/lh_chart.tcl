# lh_chart.tcl: minimal canvas-based bar chart + window capture helper.
#
#-----------------------------------------------------------------------
# Copyright: 2012-2016 (lh_chart by Dewey Garrett <dgarrett@panix.com>
#            and later contributors)
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#-----------------------------------------------------------------------

#-----------------------------------------------------------------------
# lh_chart: minimal canvas-based bar chart implementing the subset of
# blt::barchart used by latency-histogram and hal-histogram. Goal:
# visual + behavioral parity without depending on BLT (which has no
# Tcl/Tk 9 port).
#
# Public API (used at $w via command rename + dispatch):
#   $w axis    configure x|y -min -max -majorticks -logscale -hide -showticks
#   $w element create|configure NAME -xdata -ydata -fg -bg -barwidth -stipple
#   $w element exists NAME
#   $w legend  configure -hide 0|1
# Constructor:
#   lh_chart::create $w -title T -width W -height H \
#                       -plotbackground COLOR -cursor C
#-----------------------------------------------------------------------
namespace eval lh_chart {
    variable state
    variable stipple_map
    array set stipple_map {}
}

proc lh_chart::install_stipples {} {
    variable stipple_map
    if {[info exists stipple_map(_installed)]} return
    set dir /tmp/lh_chart_stipples_[pid]
    catch {file mkdir $dir}
    # Tk's XBM reader is picky: needs static char (signed, not unsigned)
    # and the data block in K&R-style continuation form with a trailing
    # closing-brace-semicolon on its own line. One-liners are rejected.
    foreach {name bits} {
        pbmap {0xe3 0xf1 0xf8 0x7c 0x3e 0x1f 0x8f 0xc7}
        nbmap {0xc7 0x8f 0x1f 0x3e 0x7c 0xf8 0xf1 0xe3}
    } {
        set fp [open $dir/$name.xbm w]
        puts $fp "#define ${name}_width 8"
        puts $fp "#define ${name}_height 8"
        puts $fp "static char ${name}_bits\[\] = \{"
        puts $fp "  [join $bits {, }]\};"
        close $fp
        set stipple_map($name) "@$dir/$name.xbm"
    }
    set stipple_map(_installed) 1
}

proc lh_chart::create {w args} {
    variable state
    install_stipples
    array set opts {
        -title           ""
        -width           480
        -height          384
        -plotbackground  honeydew1
        -cursor          arrow
    }
    array set opts $args
    canvas $w \
        -width  $opts(-width) \
        -height $opts(-height) \
        -bg     "#d9d9d9" \
        -bd 0 -highlightthickness 0 \
        -cursor $opts(-cursor)
    set state($w,title)        $opts(-title)
    set state($w,width)        $opts(-width)
    set state($w,height)       $opts(-height)
    set state($w,plotbg)       $opts(-plotbackground)
    set state($w,xmin)         -1.0
    set state($w,xmax)          1.0
    set state($w,ymin)          0.0
    set state($w,ymax)          1.0
    set state($w,ylogscale)     0
    set state($w,xticks)        {}
    set state($w,elements)      {}
    set state($w,legend_hide)   1
    set state($w,dirty)         1
    set state($w,redraw_pending) 0
    bind $w <Configure> [list lh_chart::on_configure $w]
    rename ::$w ::lh_chart::_orig_$w
    proc ::$w {args} "::lh_chart::dispatch $w {*}\$args"
    lh_chart::schedule_redraw $w
    return $w
}

proc lh_chart::dispatch {w args} {
    set sub [lindex $args 0]
    set rest [lrange $args 1 end]
    switch -- $sub {
        axis    { return [lh_chart::cmd_axis    $w {*}$rest] }
        element { return [lh_chart::cmd_element $w {*}$rest] }
        legend  { return [lh_chart::cmd_legend  $w {*}$rest] }
        default { return [::lh_chart::_orig_$w {*}$args] }
    }
}

proc lh_chart::cmd_axis {w sub which args} {
    variable state
    if {$sub ne "configure"} { error "lh_chart axis: unsupported subcommand $sub" }
    array set opts $args
    if {$which eq "x"} {
        if {[info exists opts(-min)]}        { set state($w,xmin)   $opts(-min) }
        if {[info exists opts(-max)]}        { set state($w,xmax)   $opts(-max) }
        if {[info exists opts(-majorticks)]} { set state($w,xticks) $opts(-majorticks) }
    } elseif {$which eq "y"} {
        if {[info exists opts(-logscale)]}   { set state($w,ylogscale) $opts(-logscale) }
        if {[info exists opts(-min)]}        { set state($w,ymin) $opts(-min) }
        if {[info exists opts(-max)]}        { set state($w,ymax) $opts(-max) }
    }
    schedule_redraw $w
}

proc lh_chart::cmd_element {w op name args} {
    variable state
    variable stipple_map
    switch -- $op {
        exists {
            return [expr {[lsearch -exact $state($w,elements) $name] >= 0}]
        }
        create {
            if {[lsearch -exact $state($w,elements) $name] < 0} {
                lappend state($w,elements) $name
            }
            set state($w,el,$name,xdata)    {}
            set state($w,el,$name,ydata)    {}
            set state($w,el,$name,fg)       black
            set state($w,el,$name,bg)       lightblue
            set state($w,el,$name,barwidth) 1.0
            set state($w,el,$name,stipple)  {}
            cmd_element_apply $w $name $args
        }
        configure {
            cmd_element_apply $w $name $args
        }
        default { error "lh_chart element: unsupported op $op" }
    }
    schedule_redraw $w
}

proc lh_chart::cmd_element_apply {w name optlist} {
    variable state
    variable stipple_map
    array set opts $optlist
    foreach {k storekey} {
        -xdata    xdata
        -ydata    ydata
        -fg       fg
        -bg       bg
        -barwidth barwidth
    } {
        if {[info exists opts($k)]} {
            set state($w,el,$name,$storekey) $opts($k)
        }
    }
    if {[info exists opts(-stipple)]} {
        set s $opts(-stipple)
        if {[info exists stipple_map($s)]} { set s $stipple_map($s) }
        set state($w,el,$name,stipple) $s
    }
    if {[info exists opts(-fg)] || [info exists opts(-bg)] \
        || [info exists opts(-stipple)]} {
        # Style changed: repaint existing bar items on the next redraw.
        set state($w,el,$name,restyle) 1
    }
}

proc lh_chart::cmd_legend {w sub args} {
    variable state
    if {$sub eq "configure"} {
        array set opts $args
        if {[info exists opts(-hide)]} { set state($w,legend_hide) $opts(-hide) }
    }
}

proc lh_chart::on_configure {w} {
    variable state
    if {![info exists state($w,width)]} return
    set state($w,width)  [winfo width $w]
    set state($w,height) [winfo height $w]
    schedule_redraw $w
}

proc lh_chart::schedule_redraw {w} {
    variable state
    set state($w,dirty) 1
    if {$state($w,redraw_pending)} return
    set state($w,redraw_pending) 1
    after idle [list lh_chart::redraw $w]
}

proc lh_chart::redraw {w} {
    variable state
    set state($w,redraw_pending) 0
    if {![winfo exists $w]} return
    if {!$state($w,dirty)} return
    set state($w,dirty) 0

    set W $state($w,width)
    set H $state($w,height)
    set ml 55 ; set mr 18 ; set mt 20 ; set mb 42
    set pw [expr {$W - $ml - $mr}]
    set ph [expr {$H - $mt - $mb}]
    if {$pw < 50 || $ph < 50} return
    # Data inset: keep off-chart end bars (red-stippled at xmin/xmax)
    # visibly inward from the border, matching BLT's plot margin.
    set pad 8
    set ml_d [expr {$ml + $pad}]
    set mt_d [expr {$mt + $pad}]
    set pw_d [expr {$pw - 2*$pad}]
    set ph_d [expr {$ph - 2*$pad}]

    set xmin $state($w,xmin)
    set xmax $state($w,xmax)
    set xrange [expr {double($xmax - $xmin)}]
    if {$xrange == 0} { set xrange 1.0 }

    # auto-scale Y from element data
    set ymax_data 1.0
    foreach name $state($w,elements) {
        foreach v $state($w,el,$name,ydata) {
            if {$v > $ymax_data} { set ymax_data $v }
        }
    }
    if {$state($w,ylogscale)} {
        set ymin 1.0
        if {$ymax_data < 10} {
            set ymax 10.0
        } else {
            # Nice ceiling: 1, 2, 5 times the decade — keeps Y range
            # tight so a small data growth doesn't jump a full decade.
            set decade [expr {pow(10, floor(log10($ymax_data)))}]
            set ratio  [expr {$ymax_data / $decade}]
            if {$ratio <= 1.0} {
                set ymax $decade
            } elseif {$ratio <= 2.0} {
                set ymax [expr {2.0 * $decade}]
            } elseif {$ratio <= 5.0} {
                set ymax [expr {5.0 * $decade}]
            } else {
                set ymax [expr {10.0 * $decade}]
            }
        }
    } else {
        set ymin 0.0
        # Nice ceiling for ymax: pick 1, 2, 2.5, 5, or 10 times a decade
        # so the 5 equal ticks become round numbers (e.g. 0, 600, 1200,
        # 1800, 2400, 3000) instead of (0, 580, 1160, ...).
        if {$ymax_data <= 0} {
            set ymax 1.0
        } else {
            set goal [expr {$ymax_data * 1.05}]
            set decade [expr {pow(10, floor(log10($goal)))}]
            set ratio [expr {$goal / $decade}]
            if {$ratio <= 1.0} {
                set ymax $decade
            } elseif {$ratio <= 2.0} {
                set ymax [expr {2.0 * $decade}]
            } elseif {$ratio <= 2.5} {
                set ymax [expr {2.5 * $decade}]
            } elseif {$ratio <= 5.0} {
                set ymax [expr {5.0 * $decade}]
            } else {
                set ymax [expr {10.0 * $decade}]
            }
        }
    }
    set state($w,ymin) $ymin
    set state($w,ymax) $ymax

    # Tk 9 leaks memory for every deleted canvas item (Tk 8.6 does not),
    # so steady-state redraws must not delete anything: bars are
    # persistent items repositioned via `coords` (which does not leak),
    # and the static scene (gridlines, axes, tick labels) is recreated
    # only when the layout signature changes (Y auto-scale, resize).
    # Bar items are recreated only when a bar count/stipple structure
    # changes. See create_bars/layout/update_bars.
    set lsig [list $W $H $xmin $xmax $ymin $ymax $state($w,ylogscale) \
                   $state($w,xticks) $state($w,title) $state($w,plotbg)]
    set bsig {}
    foreach name $state($w,elements) {
        lappend bsig $name \
            [llength $state($w,el,$name,xdata)] \
            [expr {$state($w,el,$name,stipple) ne ""}]
    }
    if {![info exists state($w,layout_sig)] || $lsig ne $state($w,layout_sig)} {
        set rebars [expr {![info exists state($w,bar_sig)] \
                          || $bsig ne $state($w,bar_sig)}]
        set state($w,layout_sig) $lsig
        set state($w,bar_sig) $bsig
        layout $w $ml $mt $pw $ph $ml_d $mt_d $pw_d $ph_d \
            $xmin $xmax $xrange $rebars
    } elseif {![info exists state($w,bar_sig)] || $bsig ne $state($w,bar_sig)} {
        set state($w,bar_sig) $bsig
        create_bars $w
    }
    update_bars $w $ml_d $mt_d $pw_d $ph_d $xmin $xmax $xrange
}

# Create the persistent bar items for all elements: one rectangle per
# bar (degenerate offscreen coords; update_bars positions them), plus a
# 1 px baseline line per element. Stippled bars get a bg item plus a
# stippled fg overlay item. All items carry the `bars` tag. Bars must
# stack above the gridlines but below the frame/axes/ticks (tag `top`);
# when `top` items already exist the new bars are lowered beneath them
# (during a full layout `top` is drawn after this, same stacking).
proc lh_chart::create_bars {w} {
    variable state
    set c ::lh_chart::_orig_$w
    $c delete bars
    foreach name $state($w,elements) {
        set state($w,el,$name,restyle) 0
        set state($w,el,$name,lastcoords) {}
        set fg $state($w,el,$name,fg)
        set bg $state($w,el,$name,bg)
        set st $state($w,el,$name,stipple)
        set ids {}
        set n [llength $state($w,el,$name,xdata)]
        for {set k 0} {$k < $n} {incr k} {
            if {$st ne ""} {
                set bgid [$c create rectangle -10 -10 -10 -10 \
                    -fill $bg -outline $bg -width 0 -tags bars]
                set fgid [$c create rectangle -10 -10 -10 -10 \
                    -fill $fg -outline $fg -width 0 -stipple $st -tags bars]
                lappend ids [list $bgid $fgid]
            } else {
                lappend ids [list [$c create rectangle -10 -10 -10 -10 \
                    -fill $fg -outline $fg -width 0 -tags bars]]
            }
        }
        set state($w,el,$name,barids) $ids
        # Continuous baseline: 1 px line in the element's fg color along
        # the bottom of the data area, so the bottom doesn't look broken
        # where bins have zero counts. Coords set by update_bars.
        set state($w,el,$name,baseid) \
            [$c create line -10 -10 -10 -10 -fill $fg -tags bars]
    }
    if {[llength [$c find withtag top]] > 0} {
        $c lower bars top
    }
}

# Rebuild the static scene (background, gridlines, frame, axes, tick
# labels): delete `static`-tagged items and redraw them. Persistent
# bars are kept unless $rebars (bar count/structure changed) and are
# re-stacked between gridlines and frame. Called on layout-signature
# change; see redraw().
proc lh_chart::layout {w ml mt pw ph ml_d mt_d pw_d ph_d xmin xmax xrange rebars} {
    variable state
    set c ::lh_chart::_orig_$w
    $c delete static

    set ymin $state($w,ymin)
    set ymax $state($w,ymax)

    # plot area background (no border yet, axis lines drawn last)
    $c create rectangle $ml $mt [expr {$ml+$pw}] [expr {$mt+$ph}] \
        -fill $state($w,plotbg) -outline "" -tags static

    # title
    if {$state($w,title) ne ""} {
        $c create text [expr {$ml + $pw/2}] [expr {$mt - 9}] \
            -text $state($w,title) -anchor center -font {Helvetica -12} \
            -tags static
    }

    # Y axis: build tick lists + draw gridlines now (ticks/labels at end).
    # Major ticks get a label and a long tick line; minor ticks at every
    # sub-decade gridline get a short tick line only (matches BLT).
    set y_ticks {}        ;# list of {value label} — major
    set y_minor_ticks {}  ;# list of values — minor
    if {$state($w,ylogscale)} {
        # Minor gridlines at 2..9 within each decade
        set d 1.0
        while {$d < $ymax + 0.1} {
            for {set k 2} {$k <= 9} {incr k} {
                set v [expr {$d * $k}]
                if {$v > $ymax + 0.1} break
                set y [lh_chart::ymap_log $mt_d $ph_d $ymin $ymax $v]
                $c create line $ml $y [expr {$ml+$pw}] $y \
                    -fill gray70 -dash {1 1} -tags static
                lappend y_minor_ticks $v
            }
            set d [expr {$d * 10}]
        }
        # Major gridlines at each decade
        set d 1.0
        set exp 0
        while {$d <= $ymax + 0.001} {
            set y [lh_chart::ymap_log $mt_d $ph_d $ymin $ymax $d]
            $c create line $ml $y [expr {$ml+$pw}] $y -fill gray70 -dash {1 1} \
                -tags static
            lappend y_ticks [list $d "1E$exp"]
            set d [expr {$d * 10}]
            incr exp
        }
        # Cap tick at ymax if it sits between decades (2x or 5x of decade)
        set top_decade [expr {pow(10, floor(log10($ymax) + 1e-9))}]
        set top_ratio  [expr {$ymax / $top_decade}]
        if {$top_ratio > 1.5} {
            set top_exp [expr {int(floor(log10($ymax) + 1e-9))}]
            set top_mant [expr {int(round($top_ratio))}]
            lappend y_ticks [list $ymax "${top_mant}E$top_exp"]
        }
    } else {
        set steps 5
        for {set i 0} {$i <= $steps} {incr i} {
            set v [expr {$ymin + ($ymax - $ymin) * $i / double($steps)}]
            set y [lh_chart::ymap_lin $mt_d $ph_d $ymin $ymax $v]
            $c create line $ml $y [expr {$ml+$pw}] $y -fill gray80 -dash {2 2} \
                -tags static
            lappend y_ticks [list $v [lh_chart::fmt_num $v]]
        }
    }

    # Bars stack above the background + gridlines drawn so far: raise
    # the persistent bars back, or recreate them on a structure change
    # (new items land on top, which is the right level at this point).
    if {$rebars} {
        create_bars $w
    } else {
        $c raise bars static
    }

    # Plot frame: 3D raised look. Only TOP and LEFT have a black outline
    # (the lit edges); BOTTOM and RIGHT are left without an outer black
    # line. Inside, top+left have a darker shadow line and bottom+right
    # a lighter highlight, giving the panel-edge relief BLT used.
    # Frame, axes and ticks stay on top of the bars; the `top` tag lets
    # create_bars lower later-recreated bars back beneath them.
    set xR [expr {$ml+$pw}]
    set yB [expr {$mt+$ph}]
    $c create line $ml $mt $xR $mt -fill black -tags {static top}
    $c create line $ml $mt $ml $yB -fill black -tags {static top}
    $c create line [expr {$ml+1}] [expr {$mt+1}] [expr {$xR-1}] [expr {$mt+1}] \
        -fill gray45 -tags {static top}
    $c create line [expr {$ml+1}] [expr {$mt+1}] [expr {$ml+1}] [expr {$yB-1}] \
        -fill gray45 -tags {static top}
    $c create line [expr {$ml+1}] [expr {$yB-1}] [expr {$xR-1}] [expr {$yB-1}] \
        -fill white -tags {static top}
    $c create line [expr {$xR-1}] [expr {$mt+1}] [expr {$xR-1}] [expr {$yB-1}] \
        -fill white -tags {static top}

    # Axis line: separate black line OUTSIDE the plot border, with a
    # small gap between them. Spans only the data-inset range so its
    # endpoints sit right at the topmost and bottommost ticks (BLT
    # behavior — the axis "ends with the last tick").
    set axis_gap   4
    set tick_long  10
    set tick_short 5
    set axis_x [expr {$ml - $axis_gap}]   ;# left axis (Y)
    set axis_y [expr {$yB + $axis_gap}]   ;# bottom axis (X)
    set axis_top    $mt_d                 ;# = $mt + pad
    set axis_bottom [expr {$mt_d + $ph_d}]
    set axis_left   $ml_d                 ;# = $ml + pad
    set axis_right  [expr {$ml_d + $pw_d}]
    $c create line $axis_x $axis_top $axis_x $axis_bottom -fill black \
        -tags {static top}
    $c create line $axis_left $axis_y $axis_right $axis_y -fill black \
        -tags {static top}

    # Tick marks attach to (touch) the axis line and point OUTWARD
    # toward the labels. Major ticks long, minor ticks (Y only) short.
    foreach v $y_minor_ticks {
        set y [lh_chart::ymap_log $mt_d $ph_d $ymin $ymax $v]
        $c create line [expr {$axis_x - $tick_short}] $y $axis_x $y \
            -fill black -tags {static top}
    }
    foreach pair $y_ticks {
        lassign $pair v label
        if {$state($w,ylogscale)} {
            set y [lh_chart::ymap_log $mt_d $ph_d $ymin $ymax $v]
        } else {
            set y [lh_chart::ymap_lin $mt_d $ph_d $ymin $ymax $v]
        }
        $c create line [expr {$axis_x - $tick_long}] $y $axis_x $y \
            -fill black -tags {static top}
        $c create text [expr {$axis_x - $tick_long - 2}] $y \
            -text $label -anchor e -font {Helvetica -10} -tags {static top}
    }
    set xticks $state($w,xticks)
    if {[llength $xticks] == 0} {
        set xticks [list $xmin [expr {($xmin+$xmax)/2.0}] $xmax]
    }
    foreach t $xticks {
        if {$t < $xmin - 1e-9 || $t > $xmax + 1e-9} continue
        set x [lh_chart::xmap $ml_d $pw_d $xmin $xrange $t]
        $c create line $x $axis_y $x [expr {$axis_y + $tick_long}] \
            -fill black -tags {static top}
        $c create text $x [expr {$axis_y + $tick_long + 2}] \
            -text [format %g $t] -anchor n -font {Helvetica -10} \
            -tags {static top}
    }
}

# Position the persistent bar items from current element data. Pure
# `coords` (+ rare `itemconfigure` on style change) — no item deletion,
# so this is safe to run at the full data update rate on Tk 9.
proc lh_chart::update_bars {w ml_d mt_d pw_d ph_d xmin xmax xrange} {
    variable state
    set c ::lh_chart::_orig_$w
    set ymin $state($w,ymin)
    set ymax $state($w,ymax)
    set y0 [expr {$state($w,ylogscale) \
                  ? [lh_chart::ymap_log $mt_d $ph_d $ymin $ymax 1.0] \
                  : [lh_chart::ymap_lin $mt_d $ph_d $ymin $ymax 0.0]}]
    foreach name $state($w,elements) {
        set xd $state($w,el,$name,xdata)
        set yd $state($w,el,$name,ydata)
        set bw $state($w,el,$name,barwidth)
        set hbw [expr {$bw / 2.0}]
        # Baseline follows the layout (y0), not the data, so update it
        # every pass. An element with no data has no baseline.
        if {[llength $xd] > 0} {
            $c coords $state($w,el,$name,baseid) \
                [lh_chart::xmap $ml_d $pw_d $xmin $xrange $xmin] $y0 \
                [lh_chart::xmap $ml_d $pw_d $xmin $xrange $xmax] $y0
        } else {
            $c coords $state($w,el,$name,baseid) -10 -10 -10 -10
        }
        # One coord tuple per bar; zero/out-of-range bars get degenerate
        # offscreen coords so the item count stays constant.
        set newcoords {}
        foreach x $xd y $yd {
            if {$y <= 0} {
                lappend newcoords {-10 -10 -10 -10}
                continue
            }
            if {$state($w,ylogscale) && $y < $ymin} {
                lappend newcoords {-10 -10 -10 -10}
                continue
            }
            set xa [expr {$x - $hbw}]
            set xb [expr {$x + $hbw}]
            if {$xb < $xmin || $xa > $xmax} {
                lappend newcoords {-10 -10 -10 -10}
                continue
            }
            if {$xa < $xmin} { set xa $xmin }
            if {$xb > $xmax} { set xb $xmax }
            set pxa [lh_chart::xmap $ml_d $pw_d $xmin $xrange $xa]
            set pxb [lh_chart::xmap $ml_d $pw_d $xmin $xrange $xb]
            # Pixel-snap so sub-pixel bars (e.g. 0.1us bins at ~1.2 px each)
            # always paint at least one full pixel and adjacent bars touch.
            set pxa [expr {int(floor($pxa))}]
            set pxb [expr {int(ceil($pxb))}]
            if {$pxb <= $pxa} { set pxb [expr {$pxa + 1}] }
            # Off-chart (stippled) end-of-range bars: minimum 2 px so the
            # stipple pattern is actually visible and matches BLT.
            if {$state($w,el,$name,stipple) ne "" && [expr {$pxb - $pxa}] < 2} {
                set pxb [expr {$pxa + 2}]
            }
            if {$state($w,ylogscale)} {
                set py [lh_chart::ymap_log $mt_d $ph_d $ymin $ymax $y]
            } else {
                set py [lh_chart::ymap_lin $mt_d $ph_d $ymin $ymax $y]
            }
            lappend newcoords [list $pxa $py $pxb $y0]
        }
        set ids $state($w,el,$name,barids)
        # Skip the canvas calls when coords and style are unchanged
        # (most redraws change nothing: zero bins stay zero).
        if {[info exists state($w,el,$name,lastcoords)] \
            && $state($w,el,$name,lastcoords) eq $newcoords \
            && !$state($w,el,$name,restyle)} {
            continue
        }
        set state($w,el,$name,lastcoords) $newcoords
        if {$state($w,el,$name,restyle)} {
            set state($w,el,$name,restyle) 0
            set fg $state($w,el,$name,fg)
            set bg $state($w,el,$name,bg)
            set st $state($w,el,$name,stipple)
            foreach entry $ids {
                if {[llength $entry] == 2} {
                    lassign $entry bgid fgid
                    $c itemconfigure $bgid -fill $bg -outline $bg
                    $c itemconfigure $fgid -fill $fg -outline $fg -stipple $st
                } else {
                    $c itemconfigure [lindex $entry 0] -fill $fg -outline $fg
                }
            }
        }
        foreach entry $ids co $newcoords {
            foreach id $entry {
                $c coords $id {*}$co
            }
        }
    }
}

proc lh_chart::fmt_num {v} {
    # Format a number for axis labels. Switch to sci notation (e.g.
    # 2E5, 1.5E6) once we'd otherwise need 5+ digits, so labels stay
    # narrow enough to fit in the left margin.
    if {$v == 0} { return "0" }
    set av [expr {abs($v)}]
    if {$av >= 10000 || $av < 0.01} {
        set exp [expr {int(floor(log10($av) + 1e-9))}]
        set mant [expr {$v / pow(10, $exp)}]
        if {abs($mant - round($mant)) < 0.05} {
            return [format "%dE%d" [expr {int(round($mant))}] $exp]
        } else {
            return [format "%.1fE%d" $mant $exp]
        }
    }
    if {$v == int($v)} { return [format %.0f $v] }
    return [format %g $v]
}

proc lh_chart::xmap {ml pw xmin xrange v} {
    return [expr {$ml + ($v - $xmin) / $xrange * $pw}]
}
proc lh_chart::ymap_lin {mt ph ymin ymax v} {
    set r [expr {$ymax - $ymin}]
    if {$r == 0} { set r 1 }
    return [expr {$mt + $ph - ($v - $ymin) / double($r) * $ph}]
}
proc lh_chart::ymap_log {mt ph ymin ymax v} {
    if {$v < $ymin} { set v $ymin }
    set lmin [expr {log10($ymin)}]
    set lmax [expr {log10($ymax)}]
    set r [expr {$lmax - $lmin}]
    if {$r == 0} { set r 1 }
    set lv [expr {log10($v)}]
    return [expr {$mt + $ph - ($lv - $lmin) / $r * $ph}]
}

#------------------------------------------------------------------
# Window capture without BLT. Strategy:
#   1. Tk 8.7+/9: image create photo -format window can grab a window.
#   2. Else: use ImageMagick `import -window <id>` to a temp PNG.
# Returns a tk photo image; caller is responsible for `image delete`.
proc lh_chart::capture_window {win} {
    update idletasks
    set img [image create photo]
    if {![catch {$img read $win -format window} err]} {
        return $img
    }
    image delete $img
    set wid [winfo id $win]
    set tmp /tmp/lh_snap_[pid]_[clock clicks].png
    if {[catch {exec import -window $wid $tmp} err]} {
        catch {file delete $tmp}
        error "screenshot needs ImageMagick (apt install imagemagick): $err"
    }
    set img [image create photo -file $tmp]
    catch {file delete $tmp}
    return $img
}
#------------------------------------------------------------------
