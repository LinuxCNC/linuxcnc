from math import sqrt

def flowsnake(level, startX, startY, endX, endY):
  if level == 0:
    print"g1 f10 x", endX, "y", endY
  else:
    p1X = (startX * 2 + endX)/3
    p1Y = (startY * 2 + endY)/3

    p2X = (startX + endX)/2 + (endY - startY)/sqrt(12.0)
    p2Y = (startY + endY)/2 - (endX - startX)/sqrt(12.0)

    p3X = (startX + 2 * endX)/3
    p3Y = (startY + 2 * endY)/3

    flowsnake(level-1, startX, startY, p1X, p1Y)
    flowsnake(level-1, p1X, p1Y, p2X, p2Y)
    flowsnake(level-1, p2X, p2Y, p3X, p3Y)
    flowsnake(level-1, p3X, p3Y, endX, endY)

print "S1M3"
print "g0 z1"
print "g0 x.25 y1.0"
print "g1 f10 z0"
flowsnake(5, .25, 1.0, 3.75, 1.0)
flowsnake(5, 3.75, 1.0, 2.0, 3.95)
flowsnake(5, 2.0, 3.95, .25, 1.0)
print "g0 z1"
print "M2"
