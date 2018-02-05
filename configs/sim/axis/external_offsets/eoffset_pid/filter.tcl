# filter.tcl (requires [HAL]TWOPASS)

# insert a filter on feedback signal (E:feedback)
# typcal before:
#        torchvoltage -> E:feedback == zo.feedback
# typcal after:
#        torchvoltage -> filter -> E:feedback == zo.feedback

set filtertype $::argv
if {"$filtertype" == ""} { set filtertype default }

switch $filtertype {
  biquad {
    puts "___________________________FILTER: biquad"
    loadrt biquad names=filter
    setp filter.type    1
    # Valid:  f0 > fsample/2 
    setp filter.f0     10 ;# 10Hz
    # Valid:  2 > Q > 0.5
    setp filter.Q       0.707
    setp filter.enable  1
    # Note: settings updated by enable 0->1 transition
    #       invalid settings revert to NO filtering
  }
  default {
    puts "___________________________FILTER: lowpass singlepole"
    loadrt lowpass names=filter
    # 50Hz --> 0.2696
    # 20Hz --> 0.1181
    # 10Hz --> 0.0609
    #  5Hz --> 0.0309
    #  2Hz --> 0.0125
    #  1Hz --> 0.0063
    setp filter.gain 0.0609 ;# 10Hz
  }
}

addf   filter servo-thread 1

# unlinkp source pin for E:feedback
unlinkp sim:torch.out
net  S:torch.out <= sim:torch.out
net  S:torch.out => filter.in

# new source pin for E:feedback
net E:feedback   <= filter.out

