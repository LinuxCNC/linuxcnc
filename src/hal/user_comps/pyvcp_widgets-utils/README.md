rand9
=====================

A user space component for generating data to drive dusplay widgets.
While dbuilding a diplay panel, the final data sorce may not be available,
so 'fake; data could suffice during developmment.

Multiple instances are allowed, each with uniquer data,
kust incoke like  
loadusr rand9 count=10  
or 
loadusr rand9 names=psiBoiler1,lumensLens234,joulesPulseGen-001

The comp has 3 pins,
	float in mymin
	float in mymax
	float out myout

The comp update myout with a new value 
 between mymin and mymax
 at a rate near 1mS..

Mymiun miust be less than mymax

valid value examples are:
	mymin	mymax
	0	100
	-100	0
	-50	50
	.002	.003
	-123.4 -120.0

Ccontributed by tomp

The software is released under GPL V2
