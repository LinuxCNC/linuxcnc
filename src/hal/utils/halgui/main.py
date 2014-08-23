
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

from optparse import OptionParser
import gtk

from app import Application
from load import file_load

import data
import load
import save

def main():
	usage = "usage: %prog [-c CRAPFILE] | [CRAPFILE]"
	parser = OptionParser(usage)

	(options, args) = parser.parse_args()
	if len(args) > 1:
		parser.error("incorrect number of arguments")

	app = Application()
	if len(args):
		file_load(app.design, args[0])
		app.show_app()
		gtk.main()
	else:
		app.design.update()
		app.show_app()
		gtk.main()

if __name__ == '__main__':
	try:
		import psyco
		psyco.full()
	except ImportError:
		pass
	main()

