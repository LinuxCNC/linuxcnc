import hal
import emctask

class UserFuncs(object):
    ''' additional user-defined methods for Task() which may be called through
    the IO_PLUGIN_CALL mechanism
    a mixin class to CustomTask

    methods  are expected to return a emctask,
    '''
    def __init__(self):
        print "UserFuncs.__init__()"
        myhal = hal.component("myhal")
        myhal.newpin("bit", hal.HAL_BIT, hal.HAL_OUT)
        myhal.newpin("float", hal.HAL_FLOAT, hal.HAL_OUT)
        myhal.newpin("int", hal.HAL_S32, hal.HAL_OUT)
        myhal.ready()
        self.myhal = myhal #FIXME
        self.components["myhal"] = myhal

    def demo(self,*args, **kwargs):
        print "TASK: demo(%s,%s)" % (args,kwargs)
        for i in range(int(args[0])):
            self.myhal['bit'] = not  self.myhal['bit']
        return emctask.RCS_STATUS.RCS_DONE


    def show_emcstat(self,args):
        '''
        snapshot some of emcstat to stdout
        '''
        e = emctask.emcstat
        print "mode=",e.task.mode
        print "state=",e.task.state
        print "file=",e.task.file
        print "toolOffset=",str(e.task.toolOffset)
        print "tooltable[0]=",e.io.tool.toolTable[0]
        print "g5x_offset=", e.task.g5x_offset, "system=",e.task.g5x_index
        return emctask.RCS_STATUS.RCS_DONE

    def set_named_pin(self,value,name):
        print "set_named_pin ",value,name
        (component,pin) = name.rsplit('.',1)
        comp = self.components[component]

        if type(comp[pin]).__name__ == 'float':
            comp[pin] = value

        if type(comp[pin]).__name__ == 'int':
            comp[pin] = int(value)

        if type(comp[pin]).__name__ == 'bool':
            comp[pin] = bool(int(value))

        return emctask.RCS_STATUS.RCS_DONE
