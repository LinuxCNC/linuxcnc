#    This is a component of AXIS, a front-end for emc
#    Copyright 2004 Jeff Epler <jepler@unpythonic.net> and
#    Chris Radek <chris@timeguy.com>
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

package require Img
package require img::png

proc make_color { c d } {
    if {![catch {winfo rgb . $c}]} {
        set d $c
    }
    proc $c {} [list return $d]
}

make_color systemwindowtext      #000000
make_color systembuttonface      #d9d9d9
make_color systemdisabledtext    #a3a3a3
make_color systembuttonhighlight #ececec
make_color systemhighlight       #08246b
make_color systemhighlighttext   #ffffff
make_color systemwindow          #ffffff

proc control {path args} { eval [concat frame $path $args -class Control] }
proc vspace {path args} { eval [concat frame $path $args -class Vspace] }
proc hspace {path args} { eval [concat frame $path $args -class Hspace] }
proc vrule {path args} { eval [concat frame $path $args -class Vrule] }
proc hrule {path args} { eval [concat frame $path $args -class Hrule] }
proc tab {path args} { eval [concat frame $path] }

proc find_image {name} {
    set initial [string index $name 0]
    foreach p $::imagedir {
        set q [file join $p ${name}.png]
        if {[file exists $q]} { return $q }
        set q [file join $p $initial ${name}.png]
        if {[file exists $q]} { return $q }
        set q [file join $p ${name}.gif]
        if {[file exists $q]} { return $q }
        set q [file join $p $initial ${name}.gif]
        if {[file exists $q]} { return $q }
    }
    error "Image $name does not exist"
}


proc load_image { name {img ""}} {
    if {$img == ""} { set img icon_$name }
    if {[lsearch [image names] $img] != -1}  { return $img }
    set file [find_image $name]
    image create photo $img -file $file
    return $img
}


