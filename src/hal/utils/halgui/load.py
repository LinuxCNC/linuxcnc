
from data import Component

def file_new(design):
	# should create a new design sheet
	design.file_name = ""

def file_load(design, filename):
	file_new(design)
	# load the file
	design.file_name = filename
	design.update()

