LinuxCNC Features v2 - native realtime CAM for LinuxCNC

Welcome to this advanced version of LinuxCNC-Features.

This is an almost complete re-write and you may want to copy it in a different directory
like ~/linuxcnc/features but ~/linuxcnc-features is allright too although it will
replace a good part of the original one.
 
Few changes to your subroutines will make them take full advantage of all the new features.
There is also some settings you can do in the beginning of features.py.



1. Installation
--------------------------------------------------------------------------------
1. Download and extract LinuxCNC-Features in the folder of your choice

	or clone git repo "github.com/FernV/linuxcnc-features.git" into ~/  .

2. Make sure features.py is executable

3. Make sure you have python-lxml installed. If not, open a terminal and copy the following command :

	sudo apt-get install python-lxml


2. Testing Stand Alone
--------------------------------------------------------------------------------
To start, open a terminal where features.py is and type : 

	./features.py

or

	In your file manager, double-click on features.py

Default catalog is catalogs/mill. In it is menu.xml, def_template.xml and defaults.ngc

Other catalogs are mill-mm, lathe, lathe-mm.
The command is : ./features.py --catalog="catalog-name".

Start linuxcnc (before or after, it does not matter) with the configuration ini you want, mill or lathe.
Ex.: 'linuxcnc ~/linuxcnc/configs/sim.axis/axis.ini' (or axis_mm.ini).

Open some examples and if all is well, enjoy.


3. Installing Embedded
--------------------------------------------------------------------------------
Install by copying the following lines in a terminal

1. Install python-lxml if not done yet.

2. Create links into /usr/share/pyshared/gladevcp/

	(change directory as required)
	
	```sh
	cd ___features_source_from_git_path___	
	features_dir=`pwd`
	cd /usr/share/pyshared/gladevcp/
	sudo ln "$features_dir"/features.py -s
	sudo ln "$features_dir"/features.glade -s
	```		
	
	Note! If you are using linuxcnc Run-in-place, consider this directory: <base>/lib/python/gladevcp (here and after).
	
3. Change hal_pythonplugin.py in /usr/share/pyshared/gladevcp/

	sudo gedit /usr/share/pyshared/gladevcp/hal_pythonplugin.py

	Add this new line anywhere :
	
		from features import Features


4. Change hal_python.xml in /usr/share/glade3/catalogs ()glade3 can be glade2)

	sudo gedit /usr/share/glade3/catalogs/hal_python.xml

	Find (it is in the beginning):
	
		<glade-widget-classes>
	
	Add after:
	
		<glade-widget-class name="Features" generic-name="features" title="features">
		
		    <properties>
		    
		        <property id="size" query="False" default="1" visible="False"/>
		        
		        <property id="spacing" query="False" default="0" visible="False"/>
		        
		        <property id="homogeneous" query="False" default="0" visible="False"/>
		        
		    </properties>
		    
		</glade-widget-class>
		

	Find :  
	
		<glade-widget-group name="python" title="HAL Python">
	
	Add after :
	
		<glade-widget-class-ref name="Features"/>
		

	IMPORTANT NOTE : when linuxcnc updates, it recreates directories and if features do not load
	you will have to redo steps 2, 3 and 4
	
	
5. Create links into /usr/lib/pymodules/python2.7/gladevcp



	```sh
	cd ___features_source_from_git_path___	
	features_dir=`pwd`
	cd /usr/lib/pymodules/python2.7/gladevcp
	sudo ln "$features_dir"/features.py -s
	sudo ln "$features_dir"/features.glade -s
	```		
	
4. Using Embedded
--------------------------------------------------------------------------------
Add these lines into your ini file inside [DISPLAY] section :
	
		GLADEVCP = -U --catalog=lathe features.ui
		FEATURES_PATH = /home/fernand/linuxcnc-features
	
	To use different features use one of the catalogs
		--catalog=lathe
		--catalog=lathe-mm
		--catalog=mill
		--catalog=mill-mm

5. Optional Translations
--------------------------------------------------------------------------------
Translation will work in Stand Alone AND Embedded modes

	Make links in your system locale directories to translation files
	
	cd /usr/share/locale/<YOUR LOCALE>/LC_MESSAGES
	
	sudo ln /<full path to features directory>/locale/<YOUR LOCALE>/LC_MESSAGES/linuxcnc-features.mo -s

Use poedit to translate strings in linuxcnc-features.po then save and copy linuxcnc-features.mo to
above path.


6.	Extending subroutines
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

	Valid params types are : string, float, int, bool (or boolean), header (or hdr), combo, items
	
	Note : you can change string, float and int types on the fly with the context menu. 
	This is usefull with variables.
	
	When using a value like #&lt;var_name&gt; use "string" because if will evaluate to 0 if "int" used or 0.0 if "float".
	
	Study examples in ini/mill/fv_circle.ini and others.
	
	Note : icons and images only need name.ext.
	
	
	
Some information in Russian can be obtained here: http://cnc-club.ru/forum/viewtopic.php?f=15&t=3124&p=72441#p72441

