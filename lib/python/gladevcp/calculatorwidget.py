#!/usr/bin/env python
# GladeVcp Widget - calculator input
# This widgets allows simple calculations.
# The result can be returned for further use.
# Origially to be used as numerical input on a touch screen.
#
# Copyright (c) 2012 Chris Morley
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

import sys,os
import math
datadir = os.path.abspath(os.path.dirname(__file__))

try:
	import gobject,gtk
except:
	print('GTK not available')
	sys.exit(1)

class Calculator(gtk.VBox):
	__gtype_name__ = 'Calculator'
	def __init__(self, *a, **kw):
		gtk.VBox.__init__(self, *a, **kw)
		self.preset_value = None
		self.eval_string=""
		self.wTree = gtk.Builder()
		self.wTree.add_from_file(os.path.join(datadir, "calculator.glade") )
		dic = {
			"on_CLR_clicked" : self.displayClr,
			"on_Pi_clicked" : self.displayPi,
			"on_Left_bracket_clicked" : self.displayLeftBracket,
			"on_Right_bracket_clicked" : self.displayRightBracket,
			"on_Seven_clicked" : self.displaySeven,
			"on_Eight_clicked" : self.displayEight,
			"on_Nine_clicked"  : self.displayNine,
			"on_Divide_clicked" : self.displayDiv,
			"on_Four_clicked" : self.displayFour,
			"on_Five_clicked" : self.displayFive,
			"on_Six_clicked" : self.displaySix,
			"on_Multiply_clicked" : self.displayMultiply,
			"on_One_clicked" : self.displayOne,
			"on_Two_clicked" : self.displayTwo,
			"on_Three_clicked" : self.displayThree,
			"on_Minus_clicked" : self.displayMinus,
			"on_Zero_clicked" : self.displayZero,
			"on_Dot_clicked" : self.displayDot,
			"on_Equal_clicked" : self.displayEqual,
			"on_Add_clicked" : self.displayAdd,
			}
		self.wTree.connect_signals( dic )
		window = self.wTree.get_object("calc_box")
		window.reparent(self)

	def set_value(self,value):
		self.delete()
		self.displayOperand(str(value))
		self.preset_value = value

	def get_value(self):
		self.compute()
		try:
			value = float(self.wTree.get_object("displayText").get_text())
		except:
			value = 0
		return value

	def get_preset_value(self):
		return self.preset_value

	def compute(self):
		print"string:",self.eval_string
		try   :
			b=str(eval(self.eval_string))
		except:
			b= "error"
			print"error string:",self.eval_string,sys.exc_info()[0]
			self.eval_string=''
		else  : self.eval_string=b
		self.wTree.get_object("displayText").set_text(b)
 
	def delete(self):
		self.eval_string=''
		self.wTree.get_object("displayText").set_text("")

	def displayOperand(self,i):
		self.eval_string=self.eval_string+i
		self.wTree.get_object("displayText").set_text(str(self.eval_string))
	
	def displayClr(self,widget):
		self.delete()

	def displayLeftBracket(self,widget):
		self.displayOperand("(")

	def displayRightBracket(self,widget):
		self.displayOperand(")")

	def displaySeven(self,widget):
		self.displayOperand("7")

	def displayEight(self,widget):
		self.displayOperand("8")

	def displayNine(self,widget):
		self.displayOperand("9")
	
	def displayFour(self,widget):
		self.displayOperand("4")

	def displayFive(self,widget):
		self.displayOperand("5")

	def displaySix(self,widget):
		self.displayOperand("6")

	def displayOne(self,widget):
		self.displayOperand("1")

	def displayTwo(self,widget):
		self.displayOperand("2")

	def displayThree(self,widget):
		self.displayOperand("3")

	def displayZero(self,widget):
		self.displayOperand("0")

	def displayDot(self,widget):
		self.displayOperand(".")	

	def displayPi(self,widget):
		self.displayOperand("math.pi")	
	
	def displayDiv(self,widget):
		self.displayOperand("/")	

	def displayMultiply(self,widget):
		self.displayOperand("*")
	
	def displayMinus(self,widget):
		self.displayOperand("-")
	
	def displayEqual(self,widget):
		self.compute()

	def displayAdd(self,widget):
		self.displayOperand("+")

def main():
    window = gtk.Dialog("My dialog",
                   None,
                   gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
                   (gtk.STOCK_CANCEL, gtk.RESPONSE_REJECT,
                    gtk.STOCK_OK, gtk.RESPONSE_ACCEPT))
    calc = Calculator()
    
    window.vbox.add(calc)
    window.connect("destroy", gtk.main_quit)
    calc.set_value(2.5)
    window.show_all()
    response = window.run()
    if response == gtk.RESPONSE_ACCEPT:
       print calc.get_value()
    else:
       print calc.get_preset_value()
    gtk.main()
if __name__ == "__main__":	
	main()
	
	
