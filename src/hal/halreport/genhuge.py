nsigs = 500
for i in range(nsigs):
    print "newsig signal%d float" % (i)


print "newg huge 1 1 1"

for i in range(nsigs):
    print "newm huge signal%d" % (i)



print "loadusr -W halreport -r 1 -i halreport.ini"
print "waitusr halreport"
