
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

