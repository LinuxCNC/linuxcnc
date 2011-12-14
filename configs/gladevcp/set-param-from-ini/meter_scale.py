import emc
import os

class HandlerClass:

    def __init__(self, halcomp,builder,useropts):
	self.builder = builder

	inifile = emc.ini(os.getenv("INI_FILE_NAME"))
	mmin = float(inifile.find("METER", "MIN") or 0.0)
	mmax = float(inifile.find("METER", "MAX") or 100.0)

        self.builder.get_object('meter').min = mmin
        self.builder.get_object('meter').max = mmax
        self.builder.get_object('label').set_label("min = %f\nmax = %f " % (mmin,mmax))


def get_handlers(halcomp,builder,useropts):
    return [HandlerClass(halcomp,builder,useropts)]
