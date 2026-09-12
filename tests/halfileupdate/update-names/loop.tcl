# a haltcl file: a continued command converts, a name built in the file
# does not
net long-signal \
    mux-gen.01.in-s32-00 \
    conv-s32-float.0.out
setp mux-gen.01.in-s32-01 2
hal setp mux-gen.01.sel-int 1
hal net h-sig conv-s32-float.1.in
for {set i 0} {$i < 2} {incr i} {
    setp conv-s32-float.$i.in 1
}
