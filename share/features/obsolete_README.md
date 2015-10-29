LinuxCNC Features v2.01 - native realtime CAM for LinuxCNC

Welcome to this update version of LinuxCNC-Features.

Changes are described in the CHANGES file.
 
Some changes to your subroutines will make them take full advantage of all the new features.
There is also some settings you can do in the beginning of features.py.


1. Installation
--------------------------------------------------------------------------------
1. Download and extract LinuxCNC-Features in the folder of your choice

	or clone git repo "github.com/FernV/linuxcnc-features.git" into ~/  .

2. Make sure you have python-lxml installed. If not, open a terminal and copy the following command :

	sudo apt-get install python-lxml


2. Testing Stand Alone
--------------------------------------------------------------------------------
To start, open a terminal where features.py is and type : 

	./features.py

or

	In your file manager, double-click on features.py

Default catalog is mill. In it's directory are menu.xml, def_template.xml and defaults.ngc

Other supplied catalog is lathe (not much done yet)

The command is : ./features.py --catalog=lathe

Open some examples files and enjoy.


3. Installing Embedded
--------------------------------------------------------------------------------
Installing has been simplified with the use of 'setup' script

First install python-lxml if not done yet. Required for setup and Features.

Simply type ./setup in a terminal

If you want to remove LinuxCNC-Features, use './setup c'

IMPORTANT NOTE : when linuxcnc updates, it changes some files to what they were before.
Just do ./setup again

	
4. Using Embedded
--------------------------------------------------------------------------------
Change to ./linuxcnc-configs/axis or ./linuxcnc-configs/gmoccapy

Start linuxcnc with one of the ini files available


5. Optional Translations
--------------------------------------------------------------------------------
Translation files are not included in the release. Will be in future release.


6. Configuring
--------------------------------------------------------------------------------
In the beginning of features.py are some values you can set to suit your taste

After features has started, select Utilities->Preferences and set your default values.
Click OK to save.


7. Extending subroutines
--------------------------------------------------------------------------------

1. Param subsitutions
	"#param_name" can be used to substitude parameters from the feature.
	
	"#self_id" - unique id made of feature Name + smallest integer id.
	

2. Eval and exec
	<eval>"hello world!"</eval>
	
	everything inside <eval> tag will be passed
	
	trought python's eval function.
	
	
	<exec>print "hello world"</exec>
	
	allmost the same but will take all printed data.
	
	
	you can use self as feature's self.

3. Including Gcode

	G-code files can be included by using one of the following functions:
	
	[DEFINITIONS]
	
	content =
	
		<eval>self.include_once("rotate-xy.ngc")</eval>
		<eval>self.include("some-include-file.inc")</eval>


4. Data types

	[SUBROUTINE] type should be lower case, short, without space. Ex : circle, rect, probe-dn

	Valid params types are : string, float, int, bool (or boolean), header, sub-header, combo, items, filename
	
	Note : you can change string, float and int types on the fly with the context menu. Usefull with variables.
	
	When using a value like #&lt;var_name&gt; use "string" because if will evaluate to 0 if "int" used or 0.0 if "float".
	
	Study files in ini directory
	
	
8. Notes
--------------------------------------------------------------------------------
Polyline is not fully implemented yet, not recommend to use now, but please evaluate.
Future development will concentrate more on ini and ngc files.
