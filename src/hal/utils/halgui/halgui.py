#!/usr/bin/env python

import sys, os

def crapmain():
	wd = os.path.dirname(os.path.abspath(sys.argv[0]))
	sys.path.insert(0, wd)
	import main
	main.main()

if __name__ == '__main__':
	crapmain()

