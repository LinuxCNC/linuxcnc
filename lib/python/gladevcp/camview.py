#!/usr/bin/env python

# GladeVcp Widget - CamView widget, showing the live stream
#                   from a web cam or other connected cameras.
#
# Copyright (c) 2016 Norbert Schechner
# Based on the DisplayImage code from Jay Rambhia 
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

import cv2 
import gtk
import gobject 
import threading
import subprocess
#import time

gtk.gdk.threads_init()

# prepared for localization
import gettext
_ = gettext.gettext

# check if opencv3 is used, so we will have to change attribut naming
from pkg_resources import parse_version
OPCV3 = parse_version(cv2.__version__) >= parse_version('3')

class CamView(gtk.VBox):
    
    '''
   
    GladeVcp Widget - CamView widget, showing the live stream
                     from a web cam or other connected cameras.
   
    Prerequities : 
    install opencv (cv2), min version 2.3 requiered!
    on Whessy and Jessie just do : 
    apt-get install python-opencv
    Will install also some depencies
    
    on Ubuntu 10.04 you will have to build from source
    see: http://docs.opencv.org/2.4/doc/tutorials/introduction/linux-install.html
   
    your camera must be recognised by v4l2, to test that I do recommend to install qv4l2
    apt-get install qv4l2
    start from terminal with qv4l2
    you can use this tool to get a live stream, you are on the right way.
    
    please make sure you have v4l2-ctl installed it will be used to get your devices
    sudo apt-get install v4l-utils
    
    To be able to open the camera settings from the App, you have to install also v4l2ucp
    sudo apt-get install v4l2ucp
    
    if you want special setting of your camera on start up, you can supply a command through the properties
    something like this:
    v4l2-ctl -d /dev/video1 -c exposure_auto=1 -c exposure_auto_priority=0 -c exposure_absolute=10
    it sets for your second camera (/dev/video1), the shutter time to manual settings and changes the shutter time to 10 ms.
    see v4l2-ctl --help for more details
    
    v4l2-ctl --list-formats-ext
    will list all supported frame sizes and frame rates for your camera
    
    '''
    __gtype_name__ = 'CamView'
    __gproperties__ = {
        'camera' : (gobject.TYPE_INT, 'Camera Number', 'if you have several cameras, select the one to use',
                    0, 8, 0, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'draw_color' : (gtk.gdk.Color.__gtype__, 'draw color', 'Sets the color of the drawn lines and circles',
                        gobject.PARAM_READWRITE),
        'frame_width' : (gobject.TYPE_INT, 'Width of captured frame', 'The width of the captured frame, see your camera settings for supported values',
                    320, 2560, 640, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'frame_height' : (gobject.TYPE_INT, 'Height of captured frame', 'The height of the captured frame, see your camera settings for supported values',
                    240, 1920, 480, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'circle_size' : (gobject.TYPE_INT, 'Circle Size', 'The size of the largest circle',
                    8, 1000, 150, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'number_of_circles' : (gobject.TYPE_INT, 'Number Of Circles', 'The number of circles to be drawn',
                    1, 25, 5, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'autosize' : (gobject.TYPE_BOOLEAN, 'Autosize Image', 'If checked the image will be autosized to fit best the place',
                    False, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'cam_settings' : ( gobject.TYPE_STRING, 'settings', 'Sets special camera options with a valid v4l2-ctl command',
                    "", gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),

                       }
    __gproperties = __gproperties__

    __gsignals__ = {
                     'clicked': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_STRING, gobject.TYPE_PYOBJECT)),
                     'exit': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),
                     }

    
    
    def __init__(self,videodevice=0, frame_width=640, frame_height=480):
        super(CamView, self).__init__()

        self.__version__ = "0.1.2"

        # set the selected camera as video device
        self.videodevice = videodevice
        
        # set the capture size
        self.frame_width = frame_width
        self.frame_height = frame_height

        # set other default values or initialize them
        self.color =(0, 0, 255)
        self.radius = 150
        self.radius_difference = 10
        self.circles = 5
        self.autosize = False
        self.full_info = False
        self.linewidth = 1
        #self.frame = None
        self.img_gtk = None
        self.paused = False
        self.thrd = None
        self.initialized = False

        self.img_width = self.frame_width
        self.img_height = self.frame_height
        self.img_ratio = float(self.img_width) / float(self.img_height)
        
        self.colorseldlg = None
        self.old_frames = 0

        # set the correct camera as video device
        self.cam = cv2.VideoCapture(self.videodevice)
        
        # 0  = CAP_PROP_POS_MSEC        Current position of the video file in milliseconds.
        # 1  = CAP_PROP_POS_FRAMES      0-based index of the frame to be decoded/captured next.
        # 2  = CAP_PROP_POS_AVI_RATIO   Relative position of the video file
        # 3  = CAP_PROP_FRAME_WIDTH     Width of the frames in the video stream.
        # 4  = CAP_PROP_FRAME_HEIGHT    Height of the frames in the video stream.
        # 5  = CAP_PROP_FPS             Frame rate.
        # 6  = CAP_PROP_FOURCC          4-character code of codec.
        # 7  = CAP_PROP_FRAME_COUNT     Number of frames in the video file.
        # 8  = CAP_PROP_FORMAT          Format of the Mat objects returned by retrieve() .
        # 9  = CAP_PROP_MODE            Backend-specific value indicating the current capture mode.
        # 10 = CAP_PROP_BRIGHTNESS      Brightness of the image (only for cameras).
        # 11 = CAP_PROP_CONTRAST        Contrast of the image (only for cameras).
        # 12 = CAP_PROP_SATURATION      Saturation of the image (only for cameras).
        # 13 = CAP_PROP_HUE             Hue of the image (only for cameras).
        # 14 = CAP_PROP_GAIN            Gain of the image (only for cameras).
        # 15 = CAP_PROP_EXPOSURE        Exposure (only for cameras).
        # 16 = CAP_PROP_CONVERT_RGB     Boolean flags indicating whether images should be converted to RGB.
        # 17 = CAP_PROP_WHITE_BALANCE   Currently unsupported
        # 18 = CAP_PROP_RECTIFICATION   Rectification flag for stereo cameras 
        #                               (note: only supported by DC1394 v 2.x backend currently)     
        if OPCV3:
            self.cam.set(cv2.CAP_PROP_FRAME_WIDTH, self.frame_width )
            self.cam.set(cv2.CAP_PROP_FRAME_HEIGHT, self.frame_height)
        else:
            self.cam.set(cv2.cv.CV_CAP_PROP_FRAME_WIDTH, self.frame_width )
            self.cam.set(cv2.cv.CV_CAP_PROP_FRAME_HEIGHT, self.frame_height)
        
        self.cam_properties = CamProperties()
        self.cam_properties.get_devices()
        #self.cam_properties.get_resolution(self.videodevice)
        
        # make the main GUI
        self.image_box = gtk.EventBox()
        self.add(self.image_box)
        self.img_gtk = gtk.Image()
        self.image_box.add(self.img_gtk)
        self.image_box.connect("size-allocate", self._on_size_allocate)

        # add all the button to change the drawn circles as well as run and stop
        self.btbx_upper_buttons = gtk.HButtonBox()
        self.btn_circles_minus = gtk.Button("Circles -")
        self.btn_circles_minus.set_tooltip_text(_("Push to reduce the number of circles"))
        self.btn_circles_minus.connect("clicked", self.on_btn_circle_clicked, False)      
        self.btbx_upper_buttons.add(self.btn_circles_minus)

        self.btn_circles_plus = gtk.Button("Circles +")
        self.btn_circles_plus.set_tooltip_text(_("Push to increment the number of circles"))
        self.btn_circles_plus.connect("clicked", self.on_btn_circle_clicked, True)      
        self.btbx_upper_buttons.add(self.btn_circles_plus)

        self.btn_color_chooser = gtk.Button("Color\nChooser")
        self.btn_color_chooser.set_tooltip_text(_("Push to select the drawing color"))
        self.btn_color_chooser.connect("clicked", self.on_btn_color_chooser_clicked)      
        self.btbx_upper_buttons.add(self.btn_color_chooser)
        
        self.lbl_frames = gtk.Label("Cap. Frames")
        self.btbx_upper_buttons.add(self.lbl_frames)
        
        self.btn_radius_minus=gtk.Button("Radius -")
        self.btn_radius_minus.set_tooltip_text(_("Push to reduce the circles radius by 10 Pixel"))
        self.btn_radius_minus.connect("clicked", self.on_btn_radius_clicked, False)      
        self.btbx_upper_buttons.add(self.btn_radius_minus)
        
        self.btn_radius_plus=gtk.Button("Radius +")
        self.btn_radius_plus.set_tooltip_text(_("Push to increment the circles radius by 10 Pixel"))
        self.btn_radius_plus.connect("clicked", self.on_btn_radius_clicked, True)      
        self.btbx_upper_buttons.add(self.btn_radius_plus)
        
        self.add(self.btbx_upper_buttons)
        
        self.btbx_lower_buttons = gtk.HButtonBox()
        self.chk_autosize = gtk.CheckButton("autosize")
        self.chk_autosize.set_tooltip_text(_("Push to autosize the image to given space"))
        self.chk_autosize.connect("toggled", self.chk_autosize_toggled)  
        self.btbx_lower_buttons.add(self.chk_autosize)
        
        self.chk_show_full_info = gtk.CheckButton("full info")
        self.chk_show_full_info.set_tooltip_text(_("Push to show more infos to image and given space"))
        self.chk_show_full_info.connect("toggled", self.chk_show_full_info_toggled)  
        self.btbx_lower_buttons.add(self.chk_show_full_info)
        
        self.btn_run = gtk.Button("Run")
        self.btn_run.set_tooltip_text(_("Push to start live stream"))
        self.btn_run.connect("clicked", self.on_btn_run_clicked)      
        self.btbx_lower_buttons.add(self.btn_run)
        
        self.btn_stop = gtk.Button("Stop")
        self.btn_stop.set_tooltip_text(_("Push to stop live stream"))
        self.btn_stop.connect("clicked", self.on_btn_stop_clicked)      
        self.btbx_lower_buttons.add(self.btn_stop)
        
        self.adj_videodevice = gtk.Adjustment(0, 0, len(self.cam_properties.devices)-1, 1, 1, 0)
        self.adj_videodevice.connect("value_changed", self.adj_videodevice_value_changed)
        self.spn_videodevice = gtk.SpinButton(self.adj_videodevice, 0, 0)
        if len(self.cam_properties.devices)==1:
            self.spn_videodevice.set_sensitive(False)
        self.btbx_lower_buttons.add(self.spn_videodevice)
        
        store = gtk.ListStore(str)
        store.append(["None"])
        store.append(["50 Hz"])
        store.append(["60 Hz"])
        self.cmb_powerline_frequency = gtk.ComboBox(store)
        cell = gtk.CellRendererText()
        self.cmb_powerline_frequency.pack_start(cell, True)
        self.cmb_powerline_frequency.add_attribute(cell, 'text', 0)
        self.cmb_powerline_frequency.connect("changed", self.on_cmb_powerline_frequency_changed)
        self.cmb_powerline_frequency.set_active(1)
        self.btbx_lower_buttons.add(self.cmb_powerline_frequency)
     
        self.btn_settings = gtk.Button("Camera\nSettings")
        self.btn_settings.set_tooltip_text(_("Push to open v4l2ucp to set up your camera"))
        self.btn_settings.connect("clicked", self.on_btn_settings_clicked)      
        self.btbx_lower_buttons.add(self.btn_settings)
        
#        self.cmb_resolutions = gtk.combo_box_new_text()
#        self.fill_combo(self.cmb_resolutions)
#        active_res = str(int(self.frame_width)) +"x" + str(int(self.frame_height))
#        self.set_value(self.cmb_resolutions, active_res)
#        self.cmb_resolutions.connect("changed", self.cmb_resolutions_changed)
#        self.btbx_upper_buttons.add(self.cmb_resolutions)
        
        self.btn_debug = gtk.Button("Debug\nButton")
        self.btn_debug.set_tooltip_text(_("Push to senf debug command"))
        self.btn_debug.connect("clicked", self.on_btn_debug_clicked)      
        self.btbx_lower_buttons.add(self.btn_debug)

        self.add(self.btbx_lower_buttons)

        self.btbx_upper_buttons.connect("destroy", self.quit)

        self.initialized = True

        self.thread_gtk()
 
        gobject.timeout_add( 2000, self._periodic )
 
    def thread_gtk(self):
        # without this threading function camera speed was realy poor
        self.condition = threading.Condition()
        self.thrd = threading.Thread(target = self.run, name = "CamView thread")
        self.thrd.daemon = True
        self.thrd.start()

    def _periodic(self):
        FPS = (self.captured_frames - self.old_frames)/2
        if FPS < 0:
            FPS = 0
        self.old_frames = self.captured_frames
        self.lbl_frames.set_text("FPS\n" + str(FPS) )
        return True

    def run(self):
        self.btn_run.set_sensitive(False)
        self.captured_frames = 0
        while True:
            with self.condition:
                if self.paused:
                    self.condition.wait()
                    self.captured_frames = 0
            try:
                result, frame = self.cam.read()
                if result:
                    # convert the image to RGB before displaying it
                    frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                    frame = self._draw_lines(frame)
                    if self.circles != 0:
                        frame = self._draw_circles(frame)
                        frame = self._draw_text(frame)
                    elif self.full_info:
                        frame = self._draw_text(frame)
                    self.show_image(frame)
                    # we put that in a try, to avoid an error if the user 
                    # use the App 24/7 and get to large numbers
                    try:
                        self.captured_frames += 1
                    except:
                        self.captured_frames = 0
                else:
                    pass
            except KeyboardInterrupt:
                self.quit()
                break

    def resume(self):
        self.btn_stop.set_sensitive(True)
        self.btn_run.set_sensitive(False)
        if self.circles == 0:
            self.btn_circles_plus.set_sensitive(True)
        else:
		    self.btn_radius_plus.set_sensitive(True)
		    self.btn_radius_minus.set_sensitive(True)
		    self.btn_circles_plus.set_sensitive(True)
		    self.btn_circles_minus.set_sensitive(True)

        with self.condition:
            self.paused = False
            self.condition.notify()  # unblock self if waiting

    def pause(self):
        self.btn_stop.set_sensitive(False)
        self.btn_run.set_sensitive(True)
        self.btn_radius_plus.set_sensitive(False)
        self.btn_radius_minus.set_sensitive(False)
        self.btn_circles_plus.set_sensitive(False)
        self.btn_circles_minus.set_sensitive(False)

        with self.condition:
            self.paused = True  # make self block and wait

    def _draw_lines(self, frame):
        pt1 = (0, int(frame.shape[0]/2))
        pt2 = (frame.shape[1], int(frame.shape[0]/2))
        cv2.line(frame, pt1, pt2, self.color, self.linewidth, lineType=8, shift=0)
        pt1 = (int(frame.shape[1]/2), 0)
        pt2 = (int(frame.shape[1]/2), frame.shape[0])
        cv2.line(frame, pt1, pt2, self.color, self.linewidth, lineType=8, shift=0)
        return frame
    
    def _draw_circles(self, frame):
        pos = [int(frame.shape[1]/2), int(frame.shape[0]/2)]
        cv2.circle(frame,(int(pos[0]), int(pos[1])),int(self.radius),self.color, self.linewidth)
        rad_dif = self.radius/self.circles
        if rad_dif < 2:
            rad_dif = 2
         
        r = self.radius
        circles = 0
        while r > rad_dif:
            r -= rad_dif
            if r == 0:
                break
            try:
                # why here, because we have drawn one circle with max radius allready
                circles += 1
                if circles == self.circles:
                    break
                cv2.circle(frame,(int(pos[0]), int(pos[1])),int(r) ,self.color, self.linewidth)
            except:
                pass
        return frame

    def _draw_text(self, frame):
        cv2.putText(frame, _("Radius = {}".format(self.radius)), 
                    (10, 20), cv2.FONT_HERSHEY_SIMPLEX, 0.5, self.color, 1)
        cv2.putText(frame, _("Circles = {}".format(self.circles)), 
                    (10, 40), cv2.FONT_HERSHEY_SIMPLEX, 0.5, self.color, 1)
        if self.full_info:
            cv2.putText(frame, _("Frame Width = {}".format(self.frame_width)), 
                        (10, 60), cv2.FONT_HERSHEY_SIMPLEX, 0.5, self.color, 1)
            cv2.putText(frame, _("Frame Height = {}".format(self.frame_height)), 
                        (10, 80), cv2.FONT_HERSHEY_SIMPLEX, 0.5, self.color, 1)
            cv2.putText(frame, _("Image Width = {}".format(self.img_width)), 
                        (10, 100), cv2.FONT_HERSHEY_SIMPLEX, 0.5, self.color, 1)
            cv2.putText(frame, _("Image Height = {}".format(self.img_height)), 
                        (10, 120), cv2.FONT_HERSHEY_SIMPLEX, 0.5, self.color, 1)
        return frame
  
    def show_image(self, frame):
        self.img_pixbuf =gtk.gdk.pixbuf_new_from_array(frame, gtk.gdk.COLORSPACE_RGB, 8)
        if self.autosize:
            self.img_pixbuf = self.img_pixbuf.scale_simple(self.img_width, self.img_height, gtk.gdk.INTERP_BILINEAR)
        self.img_gtk.set_from_pixbuf(self.img_pixbuf)
        self.img_gtk.show()
        self.show_all()
                   
    def _on_size_allocate(self, widget, data = None):
        if not self.autosize:
            return
        width = widget.get_allocation().width
        height = widget.get_allocation().height
        ratio = float(width)/float(height)
        # we have to check if the ratio is OK, otherwise the circles are not round        
        if ratio > self.img_ratio: # width is to large
            width = int(self.img_height * self.img_ratio)
        elif ratio < self.img_ratio: # height is to large
            height = int(self.img_width / self.img_ratio)
        # if the difference is less than 5 pixel we will not react to avoid flicker effects
        if abs(width - self.img_width) < 5 and abs(height - self.img_height) < 5:
            return
        self.img_width = width
        self.img_height = height

    def quit(self, data = None):
        self.paused = True
        self.destroy()
        gtk.main_quit()

#    def fill_combo(self, combobox):
#        store = combobox.get_model()
#        store.clear()
#        for res in self.cam_properties.get_resolution(self.videodevice):
#            combobox.append_text(res)
#        
#    def set_value(self, combobox, value):
#        index = -1
#        if self.initialized:
#            self.pause()
#        store = combobox.get_model()
#        itr = store.get_iter_first()
#        while itr:
#            if value == store.get_value(itr,0):
#                index = store.get_string_from_iter(itr)
#            itr = store.iter_next(itr)
#        combobox.set_active(int(index))
#        if self.initialized:
#            self.resume()

    def on_btn_circle_clicked(self, widget, data = None):
        if data:
            self.circles += 1
            self.btn_circles_minus.set_sensitive(True)
            self.btn_radius_plus.set_sensitive(True)
            if self.radius > self.radius_difference:
                self.btn_radius_minus.set_sensitive(True)
        else:
            self.circles -= 1
            if self.circles <= 0:
                self.circles = 0
                self.btn_circles_minus.set_sensitive(False)
                self.btn_radius_plus.set_sensitive(False)
                self.btn_radius_minus.set_sensitive(False)

    def on_btn_radius_clicked(self, widget, data = None):
        if data:
            self.radius += self.radius_difference
            self.btn_radius_minus.set_sensitive(True)
        else:
            self.radius -= self.radius_difference
            if self.radius <= self.radius_difference:
                self.radius = self.radius_difference
                self.btn_radius_minus.set_sensitive(False)

    def chk_autosize_toggled(self, widget, data = None):
        self.autosize = widget.get_active()
        if not self.autosize:
            self.img_height = self.frame_height
            self.img_width = self.frame_width

    def chk_show_full_info_toggled(self, widget, data = None):
        self.full_info = widget.get_active()

    def adj_videodevice_value_changed(self, widget, data = None):
        if not self.initialized:
            return
        device = widget.get_value()
        self.pause()
        self.cam.release()
        self.videodevice = int(device)
        self.cam = cv2.VideoCapture(self.videodevice)
        self.resume()
        self.cam_properties.set_powerline_frequeny(self.videodevice, self.cmb_powerline_frequency.get_active())
#        self.cam_properties.get_resolution(self.videodevice)
#        self.fill_combo(self.cmb_resolutions)
#
#        self.frame_width = self.cam.get(cv2.cv.CV_CAP_PROP_FRAME_WIDTH)
#        self.frame_height = self.cam.get(cv2.cv.CV_CAP_PROP_FRAME_HEIGHT)
#        active_res = str(int(self.frame_width)) +"x" + str(int(self.frame_height))
#
#        self.set_value(self.cmb_resolutions, active_res)

#    def cmb_resolutions_changed(self, widget, data = None):
#        if not self.initialized:
#            return
#        res = widget.get_active_text()
#        print(res)
#        if res:
#            width = res.split("x")[0]
#            height = res.split("x")[1]
#            self.pause()
#            self.cam.release()
#            self.cam = cv2.VideoCapture(self.videodevice)
#            self.frame_width = float(width)
#            self.frame_height = float(height)
#        
#        if OPCV3:
#            self.cam.set(cv2.CAP_PROP_FRAME_WIDTH, self.frame_width )
#            self.cam.set(cv2.CAP_PROP_FRAME_HEIGHT, self.frame_height)
#        else:
#            self.cam.set(cv2.cv.CV_CAP_PROP_FRAME_WIDTH, self.frame_width )
#            self.cam.set(cv2.cv.CV_CAP_PROP_FRAME_HEIGHT,self.frame_height)
#        self.resume()

    def on_cmb_powerline_frequency_changed(self, widget, data = None):
        value = widget.get_active()
        self.cam_properties.set_powerline_frequeny(self.videodevice, value)

    def on_btn_run_clicked(self, widget, data = None):
        self.paused = False
        self.resume()

    # just stooping the live stream, without closing the app
    def on_btn_stop_clicked(self, widget, data = None):
        self.paused = True
        self.pause()

    # open the settings control
    def on_btn_settings_clicked(self, widget, data = None):
        self.cam_properties.open_settings(self.videodevice)
        
    # launch debug command
    def on_btn_debug_clicked(self, widget, data = None):
        print("Debug clicked")

    # launch color Chooser widget
    def on_btn_color_chooser_clicked(self, widget, data = None):
        self._get_color()

    def _get_color(self, data= None):
        # Create color selection dialog
        if self.colorseldlg == None:
            self.colorseldlg = gtk.ColorSelectionDialog("Select drawing color")

        # Get the ColorSelection widget
        colorsel = self.colorseldlg.colorsel
        gtk_color = self._convert_to_gtk_color(self.color)
        colorsel.set_previous_color(gtk_color)
        colorsel.set_current_color(gtk_color)
        colorsel.set_has_palette(True)

        # Show the dialog
        response = self.colorseldlg.run()

        if response -- gtk.RESPONSE_OK:
            self.color = self._convert_to_rgb(colorsel.get_current_color())
        else:
            pass

        self.colorseldlg.hide()

    # returns the separate BGR color numbers from the color
    def _convert_to_rgb(self, spec):
        color = spec.to_string()
        temp = color.strip("#")
        r = temp[0:4]
        g = temp[4:8]
        b = temp[8:]
        return (int(r, 16), int(g, 16), int(b, 16))

    def _convert_to_gtk_color(self, spec):
        def clamp(x): 
          return max(0, min(x, 255))
        
        return gtk.gdk.Color("#{0:02x}{1:02x}{2:02x}".format(clamp(spec[0]), clamp(spec[1]), clamp(spec[2])))

    # Get properties
    def do_get_property(self, property):
        name = property.name.replace('-', '_')
        if name in self.__gproperties.keys():
            return getattr(self, name)
        else:
            raise AttributeError('unknown property %s' % property.name)

    # Set properties
    def do_set_property(self, property, value):
        #print(property,value)
        try:
            name = property.name.replace('-', '_')
            if name in self.__gproperties.keys():
                if name == "camera":
                    self.videodevice = value
                if name == "draw_color":
                    self.color = self._convert_to_rgb(value)
                if name == "frame_width":
                    self.frame_width = value
                if name == "frame_height":
                    self.frame_height = value
                if name == "circle_size":
                    self.radius = value
                    self._set_labels()
                if name == "number_of_circles":
                    self.circles = value
                if name == "autosize":
                    self.autosize = value
                if name == "cam_settings":
                    self.cam_properties._run_command(value)
            else:
                raise AttributeError(_('unknown property %s or value %s') % (property.name, value) )
        except:
            pass

class CamProperties():
    
    def __init__(self):
        # get all availible devices
        self.devices = []        
        self.resolutions = []        
        
    def get_devices(self):
        result = subprocess.Popen(['v4l2-ctl', '--list-devices'], stdout=subprocess.PIPE).communicate()[0]
        result = str(result)
        infos = result.split('\n')
        for item, info in enumerate(infos):
            if info =="":
                continue
            info = info.strip(' \t\n\r')
            if "/dev/video" in info:
                device = str(info + " = " + infos[item-1])
                self.devices.append(device)
        return self.devices

#    def get_resolution(self, videodevice=0):
#        self.resolutions = []
#        
#        # get all availible resolutions
#        result = subprocess.Popen(['v4l2-ctl', '-d' + str(videodevice), '--list-formats-ext'], stdout=subprocess.PIPE)#.communicate()[0]
#        line = ""
#        res =""
#        rate = ""
#        for letter in result.stdout.read():
#            if letter == "\n":
#                #print line.strip(" \t\n\r")
#                info = line.split(":")
#                if "Size" in info[0]:
#                    res = info[1].split()
#                    #width,height = res[1].split("x")
##                if "Interval" in info[0]:
##                    rate = info[1].split("(")
##                    rate = rate[1]
##                    rate = rate.strip(" fps)")
#                    self.resolutions.append(str(res[1]))
#                line = ""
#            else:
#                line += letter
#        
##        # check for doble entries, not order preserving
#        checked = []
#        for element in self.resolutions:
#            if element not in checked:
#                checked.append(element)
#        
#        self.resolutions = sorted(checked)
#        return self.resolutions

    def set_powerline_frequeny(self, videodevice, value):
        command = 'v4l2-ctl -d'+ str(videodevice) + ' --set-ctrl=power_line_frequency=' + str(value)
        self._run_command(command)
        
    def open_settings(self, videodevice = 0):        
        command = "v4l2ucp /dev/video"+ str(videodevice)
        self._run_command(command)

    def _run_command(self, command):
        if command:
            result = subprocess.Popen(command, stderr=None, shell=True)

#    def close_child(self):
#        os.kill(self.v4l2ucp.pid, signal.SIGKILL)
#        print("kill signal emitted",self.v4l2ucp.pid)
        

if __name__ == '__main__':
    window = gtk.Window(gtk.WINDOW_TOPLEVEL)
    window.set_title("CamView Window")
    camv = CamView(videodevice=0, frame_width=640, frame_height=480)
    window.add(camv)
    camv.set_property("draw_color", gtk.gdk.Color("yellow"))
    camv.set_property("circle_size", 150)
    camv.set_property("number_of_circles", 5)
    command = 'v4l2-ctl -d0 --set-ctrl=power_line_frequency=1'
    camv.set_property("cam_settings", command)
    window.show_all()
    window.connect("destroy", camv.quit)
    gtk.main()
