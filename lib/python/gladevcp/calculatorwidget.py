#!/usr/bin/env python3
# GladeVcp Widget - calculator input
# This widgets allows simple calculations.
# The result can be returned for further use.
# Originally to be used as numerical input on a touch screen.
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

# This fixes integer division error
# so dividing two integers gives a float

import sys, os
import math

# localization
import locale
locale.setlocale( locale.LC_ALL, '' )

datadir = os.path.abspath( os.path.dirname( __file__ ) )

import gi
gi.require_version("Gtk","3.0")
from gi.repository import Gtk
from gi.repository import GObject
from gi.repository import Pango

class Calculator( Gtk.VBox ):
    __gtype_name__ = 'Calculator'
    __gproperties__ = {
        'is_editable' : ( GObject.TYPE_BOOLEAN, 'Is Editable', 'Ability to use a keyboard to enter data',
                    False, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT ),
        'font' : ( GObject.TYPE_STRING, 'Pango Font', 'Display font to use',
                "sans 12", GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT ),
    }
    __gproperties = __gproperties__

    def __init__( self, *a, **kw ):
        Gtk.VBox.__init__( self, *a, **kw )
        self.preset_value = None
        self.eval_string = ""
        self.font = "sans 12"
        self.is_editable = False
        self.integer_only = False
        self.wTree = Gtk.Builder()
        self.wTree.add_from_file( os.path.join( datadir, "calculator.glade" ) )
        dic = {
            "on_displayText_activate" : self.displayText,
            "on_displayText_changed" : self.displayText_changed,
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
            "on_Backspace_clicked" : self.displayBackspace,
            "on_mm_inch_clicked" : self.displayMmInch,
            "on_inch_mm_clicked" : self.displayInchMm            
            }
        self.wTree.connect_signals( dic )
        self.entry = self.wTree.get_object( "displayText" )
        self.entry.modify_font( Pango.FontDescription( self.font ) )
        self.calc_box = self.wTree.get_object( "calc_box" )
        window = self.wTree.get_object( "calc_box" )
        window.reparent( self )

    def num_pad_only( self, value ):
        objects = ["Left_bracket", "Right_bracket", "Pi", "Divide", "Multiply", "Add", "Minus", "Equal"]
        for i in objects:
            temp = self.wTree.get_object( i )
            if value:
                temp.hide()
            else:
                temp.show()

    def integer_entry_only( self, value ):
        temp = self.wTree.get_object( 'Dot' )
        if value:
            temp.hide()
            self.integer_only = True
        else:
            temp.show()
            self.integer_only = False

    def set_editable( self, value ):
        self.is_editable = value
        self.entry.set_editable( value )

    def set_font( self, font ):
        self.font = font
        self.entry.modify_font( Pango.FontDescription( font ) )

    def set_value( self, value ):
        val = value
        try:
            val = str( locale.format_string( "%f", float( val ) ).rstrip( "0" ) )
            if val[-1] == locale.localeconv()["decimal_point"]:
                val = val.rstrip( locale.localeconv()["decimal_point"] )
        except:
            value = "Error"
        self.delete()
        self.displayOperand( str( val ) )
        self.preset_value = value

    def get_value( self ):
        self.compute()
        try:
            value = self.wTree.get_object( "displayText" ).get_text()
            return locale.atof( value )
        except:
            return None
#        print( "value in get value = ", value )
#        print( "converted value in get value = ", locale.atof( value ) )

    def get_preset_value( self ):
        return self.preset_value

    def compute( self ):
        qualified = ''
        # print"string:",self.eval_string
        temp = self.eval_string.strip( " " ).replace("Pi", "math.pi")
        # this loop adds only spaces around the mentioned operators 
        for i in( '-', '+', '/', '*', 'math.pi', '(', ')' ):
            new = " %s " % i
            temp = temp.replace( i, new )
        for i in temp.split():
#            print ( "i in compute = ", i )
            try:
                i = str( locale.atof( i ) )
#                print ( "converted i in compute = ", i )
            except:
                pass
            if i.isdigit():
                qualified = qualified + str( float( i ) )
            else:
                qualified = qualified + i
        try   :
            if self.integer_only:
                b = str( int( eval( qualified ) ) )
            else:
                b = str( eval( qualified ) )
        except:
            b = "Error"
            print("Calculator widget error, string:", self.eval_string, sys.exc_info()[0])
            self.eval_string = ''
        else  : self.eval_string = b
        # if locale.localeconv()["decimal_point" = comma ,
        # we have to replace the internal dot by a comma,
        # otherwise it will be interpreted as an thousend separator
        try:
            b = locale.format_string( "%f", float( b ) ).rstrip( "0" )
            if b[-1] == locale.localeconv()["decimal_point"]:
                b = b.rstrip( locale.localeconv()["decimal_point"] )
        except:
            b = "Error"
        self.wTree.get_object( "displayText" ).set_text( b )
        self.eval_string = b + " " # add space to indicate that calculation was last

    def delete( self ):
        self.eval_string = ''
        self.wTree.get_object( "displayText" ).set_text( "" )

    def displayOperand( self, i ):
        if self.wTree.get_object( "displayText" ).get_selection_bounds():
            self.delete()
        if "Error" in self.eval_string:
            self.eval_string = ""
        # clear text area when entering a number after a finished calculation
        #if i.isdigit():
        if i not in "+-*/" and self.eval_string != "":
            if self.eval_string[-1] == " ":
                self.eval_string = ""
    
        self.eval_string = self.eval_string + i
        self.wTree.get_object( "displayText" ).set_text( str( self.eval_string ) )

    def displayText_changed( self, widget ):
        self.eval_string = widget.get_text()

    def displayText( self, widget ):
        pass
        # self.compute()

    def displayClr( self, widget ):
        self.delete()

    def displayBackspace( self, widget ):
        text = self.wTree.get_object( "displayText" ).get_text()
        if(text == "Error"):
            self.delete()
        else:
            if text[-2:] == "Pi":
                self.wTree.get_object( "displayText" ).set_text(text[:-2])
            else:
                self.wTree.get_object( "displayText" ).set_text(text[:-1])

    def displayLeftBracket( self, widget ):
        self.displayOperand( "(" )

    def displayRightBracket( self, widget ):
        self.displayOperand( ")" )

    def displaySeven( self, widget ):
        self.displayOperand( "7" )

    def displayEight( self, widget ):
        self.displayOperand( "8" )

    def displayNine( self, widget ):
        self.displayOperand( "9" )

    def displayFour( self, widget ):
        self.displayOperand( "4" )

    def displayFive( self, widget ):
        self.displayOperand( "5" )

    def displaySix( self, widget ):
        self.displayOperand( "6" )

    def displayOne( self, widget ):
        self.displayOperand( "1" )

    def displayTwo( self, widget ):
        self.displayOperand( "2" )

    def displayThree( self, widget ):
        self.displayOperand( "3" )

    def displayZero( self, widget ):
        self.displayOperand( "0" )

    def displayDot( self, widget ):
        self.displayOperand( locale.localeconv()["decimal_point"] )

    def displayPi( self, widget ):
        self.displayOperand( "Pi" )

    def displayDiv( self, widget ):
        self.displayOperand( "/" )

    def displayMultiply( self, widget ):
        self.displayOperand( "*" )

    def displayMinus( self, widget ):
        self.displayOperand( "-" )

    def displayEqual( self, widget ):
        self.compute()

    def displayAdd( self, widget ):
        self.displayOperand( "+" )

    def displayMmInch( self, widget ):
        self.eval_string = "("+ self.eval_string + ") / " + locale.format("%f", float(25.4))
        self.compute()

    def displayInchMm( self, widget ):
        self.eval_string = "("+ self.eval_string + ") * " + locale.format("%f", float(25.4))
        self.compute()

    def do_get_property( self, property ):
        name = property.name.replace( '-', '_' )
        if name in list(self.__gproperties.keys()):
            return getattr( self, name )
        else:
            raise AttributeError( 'unknown property %s' % property.name )

    def do_set_property( self, property, value ):
        try:
            name = property.name.replace( '-', '_' )
            if name == 'is_editable':
                self.set_editable( value )
            if name == 'font':
                self.set_font( value )
        except:
            pass

# for testing without glade editor:
def main():
    window = Gtk.Dialog( "My dialog",
                   None,
                   Gtk.DialogFlags.MODAL | Gtk.DialogFlags.DESTROY_WITH_PARENT,
                   ( Gtk.STOCK_CANCEL, Gtk.ResponseType.REJECT,
                    Gtk.STOCK_OK, Gtk.ResponseType.ACCEPT ) )
    calc = Calculator()

    window.vbox.add( calc )
    window.connect( "destroy", Gtk.main_quit )
    calc.set_value( 2.5 )
    calc.set_font( "sans 25" )
    calc.set_editable( True )
    window.show_all()
    response = window.run()
    if response == Gtk.ResponseType.ACCEPT:
       print(calc.get_value())
    else:
       print(calc.get_preset_value())

if __name__ == "__main__":
    main()


