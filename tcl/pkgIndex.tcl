package ifneeded "Emc" 1.0 \
 [list source [file join $dir emc.tcl]]
package ifneeded "Hal" 1.0 \
 [list load [file join $dir hal[info sharedlibextension]]]
