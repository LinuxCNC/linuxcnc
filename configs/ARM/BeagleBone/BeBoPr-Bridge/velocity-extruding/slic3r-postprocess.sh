#!/bin/sh

# first get the variables needed for setting width and height for
# extruding from the slic3r file
#; layer_height = 0.10
#; nozzle_diameter = 0.35
#; filament_diameter = 2.85
#; extrusion_multiplier = 1.0
#; perimeters extrusion width = 0.40mm
#; infill extrusion width = 1.10mm
#; solid infill extrusion width = 0.40mm
#; top infill extrusion width = 0.40mm
#; first_layer_height = 0.10

# fill all variables needed to change the velocity control extrudion settings
# settings at beginning, filament diameter
# nozzle diameter not yet used for anything
# meaning height of layer and width of typical job (infill, solid,
# perimeter etc) width of current line
layer_height=$(grep -m 1 '; layer_height = ' "$1" \
    | rev | cut -d ' ' -f1 | rev)
# above is line, reversed, then cut wit space delimeter, first part
# is kept, and then reversed
nozzle_diameter=$(grep -m 1 '; nozzle_diameter = ' "$1" \
    | rev | cut -d ' ' -f1 | rev)
filament_diameter=$(grep -m 1 '; filament_diameter = ' "$1" \
    | rev | cut -d ' ' -f1 | rev)
extrusion_multiplier=$(grep -m 1 '; extrusion_multiplier = ' "$1" \
    | rev | cut -d ' ' -f1 | rev)
perimeters_extrusion_width=$(grep -m 1 '; perimeters extrusion width = ' "$1" \
    | rev | cut -d ' ' -f1 | rev | sed 's|mm||')
# above line is also trimmed of the 2 tratling 'mm' characters
infill_extrusion_width=$(grep -m 1 '; infill extrusion width = ' "$1" \
    | rev | cut -d ' ' -f1 | rev | sed 's|mm||')
solid_infill_extrusion_width=$(grep -m 1 '; solid infill extrusion width = ' "$1" \
    | rev | cut -d ' ' -f1 | rev | sed 's|mm||')
top_infill_extrusion_width=$(grep -m 1 '; top infill extrusion width = ' "$1" \
    | rev | cut -d ' ' -f1 | rev | sed 's|mm||')
first_layer_height=$(grep -m 1 '; first_layer_height = ' "$1" \
    | rev | cut -d ' ' -f1 | rev)

# above would be nicer in a loop, although with the different commands (sed for the 'mm') maybe not

echo "layer height             = $layer_height"
echo "first layer height first = $first_layer_height"
echo "nozzle diameter          = $nozzle_diameter"
echo "filament diameter        = $filament_diameter"
echo "extrusion multiplier     = $extrusion_multiplier"
echo "width perimeter          = $perimeters_extrusion_width"
echo "width infill             = $infill_extrusion_width"
echo "width solid infill       = $solid_infill_extrusion_width"
echo "width top infill         = $top_infill_extrusion_width"

#instf="instellingen.txt"
#echo "layer height=$layer_height" > $instf
#echo "first layer height first=$first_layer_height" >> $instf
#echo "nozzle diameter=$nozzle_diameter" >> $instf
#echo "filament diameter=$filament_diameter" >> $instf
#echo "extrusion multiplier=$extrusion_multiplier" >> $instf
#echo "width perimeter=$perimeters_extrusion_width" >> $instf
#echo "width infill=$infill_extrusion_width" >> $instf
#echo "width solid infill=$solid_infill_extrusion_width" >> $instf
#echo "width top infill=$top_infill_extrusion_width" >> $instf


# remove all comment lines, convert all E to A axis, convert M106 to M206
sed -e 's|^;.*$||' \
    -e 's| E| A|' \
    -e 's|^M106 |M206 |' \
    "$1" > "e_axes_converted_$1"

sed '
/^M109 / a\
M66 P0 L3 Q100
' "e_axes_converted_$1" > "M66-added_$1"
rm "e_axes_converted_$1"

sed -e 's|^M109 |M104 |' \
    -e 's|^G92 A.*$||' \
    -e 's|^G1.*; retract$|M68 E4 Q0.2|' \
    -e 's|^G11 ; unretract|M68 E4 Q0.0|' \
    -e 's|^.*; compensate retraction|M68 E4 Q0.0|' \
    -e '/^$/ d' \
    "M66-added_$1" > "cleaned_$1"
rm "M66-added_$1"

python ./cleanup-for-velocity-extrusion.py \
    "cleaned_$1" "$perimeters_extrusion_width" \
    "$infill_extrusion_width" \
    "$layer_height" \
    "$first_layer_height"
rm "cleaned_$1"

# remove empty lines for next script
sed -e '/^$/ d' "result-cleaned_$1" > "cleaned_$1"
rm "result-cleaned_$1"

# now the following occurs:
# the time of the retract period is not set before the disconnect of the
# extruder with the nozzle motion. It's something in the slic3r output
# before the .py script the file is like this:
#
#G1 X-15.574 Y-18.156 A541.86412 ; fill
#G0 Z9.000 F18000.000 ; move to next layer (44)
#G1 F1800.000 A540.56412 ; retract
#
#and afterwards it's like this:
#
#G1 X-15.574 Y-18.156 A541.86412 ; fill
#M65 P2
#G0 Z9.000 F18000.000 ; move to next layer (44)
#M68 E4 Q0.2
#
python ./correct-retract-on-next-layer.py "cleaned_$1"
rm "cleaned_$1"

#convert g1 line segments to g2 or g3 if possible
g1-to-g23 "swapped-cleaned_$1" "swapped-cleaned_$1.bak"
mv "swapped-cleaned_$1.bak" "swapped-cleaned_$1"

# convert all M106 to B axis
#sed '/^M106/s/M106 P/G0 B/' "$1post" > "$1"
#rm "$1post"

# s = substitute, | = delimiter, get from $1post and write to $1
# sed 's|..|..|' "$1post" > "$1"
# first part should be A0.7777 or A-0.7777 (or more or less numbers)

# make a regular expression with space followed with an A as anchor
# the expression should then have a . with 1 or more [0-9] before and 1 or
# more [0-9] after that AND the [0-9] before the . has to have zero or
# more - signs befor that.
# -few-
# then replace it with nothing (note ||)

# remove all A's, change retract comments, and all empty lines
#sed -e 's| A[0-9\-\.]* | |'
sed -e 's| A\-*[0-9\.]* | |' \
    -e '/^$/ d' \
    "swapped-cleaned_$1" > "processed_$1"
rm "swapped-cleaned_$1"
