"""PyVCP widgets for REST/WebSocket mode — event-driven, no polling.

Each widget receives a PyVCPClient instance. Server state is pushed
via on_pin_change callbacks; user interactions send set_pin to server.

No update() method. No polling loop. Server is source of truth.
"""

import math
import sys
import time
import tkinter as Tkinter
from tkinter import *

import bwidget

from pyvcp_client import PyVCPClient


# ============================================================
# Display widgets (IN pins only — server → widget)
# ============================================================

class pyvcp_led(Canvas):
    n = 0

    def __init__(self, master, client, halpin=None, disable_pin=False,
                 off_color="red", on_color="green", disabled_color="gray80",
                 size=20, **kw):
        Canvas.__init__(self, master, width=size, height=size, bd=0)
        self.off_color = off_color
        self.on_color = on_color
        self.disabled_color = disabled_color
        self.oh = self.create_oval(1, 1, size, size)
        self.itemconfig(self.oh, fill=off_color)

        if halpin is None:
            halpin = "led." + str(pyvcp_led.n)
            pyvcp_led.n += 1
        self.halpin = halpin
        self.disable_pin = disable_pin

        client.on_pin_change(halpin, self._on_value)
        if disable_pin:
            self.halpin_disable = halpin + ".disable"
            client.on_pin_change(self.halpin_disable, self._on_disable)

    def _on_value(self, val):
        if val:
            self.itemconfig(self.oh, fill=self.on_color)
        else:
            self.itemconfig(self.oh, fill=self.off_color)

    def _on_disable(self, val):
        if val:
            self.itemconfig(self.oh, fill=self.disabled_color)


class pyvcp_rectled(Canvas):
    n = 0

    def __init__(self, master, client, halpin=None, disable_pin=False,
                 off_color="red", on_color="green", disabled_color="gray80",
                 height=10, width=30, **kw):
        Canvas.__init__(self, master, width=width, height=height, bd=2)
        self.off_color = off_color
        self.on_color = on_color
        self.disabled_color = disabled_color
        self.oh = self.create_rectangle(1, 1, width, height)
        self.itemconfig(self.oh, fill=off_color)

        if halpin is None:
            halpin = "led." + str(pyvcp_led.n)
            pyvcp_led.n += 1
        self.halpin = halpin
        self.disable_pin = disable_pin

        client.on_pin_change(halpin, self._on_value)
        if disable_pin:
            self.halpin_disable = halpin + ".disable"
            client.on_pin_change(self.halpin_disable, self._on_disable)

    def _on_value(self, val):
        if val:
            self.itemconfig(self.oh, fill=self.on_color)
        else:
            self.itemconfig(self.oh, fill=self.off_color)

    def _on_disable(self, val):
        if val:
            self.itemconfig(self.oh, fill=self.disabled_color)


class pyvcp_number(Label):
    n = 0

    def __init__(self, master, client, halpin=None, format="2.1f", **kw):
        self.v = StringVar()
        self.format = format
        Label.__init__(self, master, textvariable=self.v, **kw)
        if halpin is None:
            halpin = "number." + str(pyvcp_number.n)
            pyvcp_number.n += 1
        self.halpin = halpin
        fmt = "%(b)" + self.format
        self.v.set(str(fmt % {'b': 0.0}))
        client.on_pin_change(halpin, self._on_value)

    def _on_value(self, val):
        fmt = "%(b)" + self.format
        self.v.set(str(fmt % {'b': val}))


class pyvcp_u32(Label):
    n = 0

    def __init__(self, master, client, halpin=None, format="d", **kw):
        self.v = StringVar()
        self.format = format
        Label.__init__(self, master, textvariable=self.v, **kw)
        if halpin is None:
            halpin = "number." + str(pyvcp_number.n)
            pyvcp_number.n += 1
        self.halpin = halpin
        fmt = "%(b)" + self.format
        self.v.set(str(fmt % {'b': 0}))
        client.on_pin_change(halpin, self._on_value)

    def _on_value(self, val):
        fmt = "%(b)" + self.format
        self.v.set(str(fmt % {'b': val}))


class pyvcp_s32(Label):
    n = 0

    def __init__(self, master, client, halpin=None, format="d", **kw):
        self.v = StringVar()
        self.format = format
        Label.__init__(self, master, textvariable=self.v, **kw)
        if halpin is None:
            halpin = "number." + str(pyvcp_number.n)
            pyvcp_number.n += 1
        self.halpin = halpin
        fmt = "%(b)" + self.format
        self.v.set(str(fmt % {'b': 0}))
        client.on_pin_change(halpin, self._on_value)

    def _on_value(self, val):
        fmt = "%(b)" + self.format
        self.v.set(str(fmt % {'b': val}))


class pyvcp_timer(Label):
    n = 0

    def __init__(self, master, client, halpin=None, **kw):
        self.v = StringVar()
        Label.__init__(self, master, textvariable=self.v, **kw)
        if halpin is None:
            halpin = "timer." + str(pyvcp_timer.n)
            pyvcp_timer.n += 1
        self.v.set("00:00:00")
        self.resetvalue = 0
        self.runvalue = 0
        self.starttime = 0
        self.basetime = 0

        self._reset_pin = halpin + ".reset"
        self._run_pin = halpin + ".run"
        client.on_pin_change(self._reset_pin, self._on_reset)
        client.on_pin_change(self._run_pin, self._on_run)

        # Timer needs periodic display updates when running.
        self._master = master
        self._tick()

    def _on_reset(self, val):
        if val:
            self.basetime = 0
            self.starttime = time.time()

    def _on_run(self, val):
        if val and not self.runvalue:
            self.starttime = time.time()
        elif not val and self.runvalue:
            self.basetime += time.time() - self.starttime
        self.runvalue = val

    def _tick(self):
        if self.runvalue:
            total = self.basetime + time.time() - self.starttime
        else:
            total = self.basetime
        hr = int(total / 3600)
        remainder = total - hr * 3600
        mn = int(remainder / 60)
        sec = int(remainder - mn * 60)
        self.v.set("%02d:%02d:%02d" % (hr, mn, sec))
        self._master.after(200, self._tick)


class pyvcp_bar(Canvas):
    n = 0

    def __init__(self, master, client, fillcolor="green", bgcolor="grey",
                 halpin=None, min_=0.0, max_=100.0, range1=None, range2=None,
                 range3=None, format='3.1f', canvas_width=None,
                 canvas_height=None, bar_width=None, bar_height=None,
                 width=150, height=30, **kw):
        self.cw = width + 50
        self.ch = height + 20
        self.bw = width
        self.bh = height
        if canvas_width is not None:
            self.cw = canvas_width
        if canvas_height is not None:
            self.ch = canvas_height
        if bar_width is not None:
            self.bw = bar_width
        if bar_height is not None:
            self.bh = bar_height
        self.pad = ((self.cw - self.bw) / 2)
        Canvas.__init__(self, master, width=self.cw, height=self.ch)

        if halpin is None:
            halpin = "bar." + str(pyvcp_bar.n)
            pyvcp_bar.n += 1
        self.halpin = halpin
        self.endval = max_
        self.startval = min_
        self.format = "%" + format
        self.fillcolor = fillcolor
        self.value = 0.0

        border = self.create_rectangle(self.pad, 1, self.pad + self.bw, self.bh)
        self.itemconfig(border, fill=bgcolor)
        self.bar = self.create_rectangle(self.pad, 2, self.pad, self.bh - 1)
        self.itemconfig(self.bar, fill=fillcolor)
        self.create_text(self.pad, self.bh + 10, text=str(self.startval))
        self.create_text(self.pad + self.bw, self.bh + 10, text=str(self.endval))
        self.val_text = self.create_text(self.pad + self.bw / 2, self.bh / 2, text="0")

        if range1 is not None and range2 is not None and range3 is not None:
            self.range1, self.range2, self.range3 = range1, range2, range3
            self.ranges = True
        else:
            self.ranges = False

        client.on_pin_change(halpin, self._on_value)

    def _on_value(self, val):
        self.value = val
        valtext = str(self.format % self.value)
        self.itemconfig(self.val_text, text=valtext)
        if self.ranges:
            self._set_fill()
        start, end = self._bar_coords()
        self.coords(self.bar, start, 2, end, self.bh - 1)

    def _bar_coords(self):
        min_px = self.pad
        max_px = self.pad + self.bw
        scale = (max_px - min_px) / (self.endval - self.startval)
        bar_end = min_px + scale * (self.value - self.startval)
        bar_end = max(min_px, min(max_px, bar_end))
        bar_start = min_px + scale * (0 - self.startval)
        bar_start = max(min_px, bar_start)
        return bar_start, bar_end

    def _set_fill(self):
        for start, end, color in (self.range1, self.range2, self.range3):
            if start < self.value <= end:
                self.itemconfig(self.bar, fill=color)
                return


class pyvcp_meter(Canvas):
    n = 0

    def __init__(self, master, client, halpin=None, size=200, text=None,
                 subtext=None, min_=0, max_=100, majorscale=None,
                 minorscale=None, region1=None, region2=None, region3=None, **kw):
        self.size = size
        self.pad = 10
        Canvas.__init__(self, master, width=size, height=size)
        self.min_ = min_
        self.max_ = max_
        range_ = 2.5
        self.min_alfa = -math.pi / 2 - range_
        self.max_alfa = -math.pi / 2 + range_
        self.circle = self.create_oval(self.pad, self.pad, size - self.pad, size - self.pad, width=2)
        self.itemconfig(self.circle, fill="white")
        self.mid = size / 2
        self.r = (size - 2 * self.pad) / 2
        if minorscale is None:
            self.minorscale = 0
        else:
            self.minorscale = minorscale
        if majorscale is None:
            self.majorscale = float((self.max_ - self.min_) / 10)
        else:
            self.majorscale = majorscale
        if text is not None:
            self.create_text([self.mid, self.mid - size / 12],
                             font="Arial %d bold" % (size / 10), text=text)
        if subtext is not None:
            self.create_text([self.mid, self.mid + size / 12],
                             font="Arial %d" % (size / 30 + 5), text=subtext)
        if region1 is not None:
            self._draw_region(region1)
        if region2 is not None:
            self._draw_region(region2)
        if region3 is not None:
            self._draw_region(region3)
        self._draw_ticks()
        self.line = self.create_line(
            [self.mid, self.mid, self.mid + self.r, self.mid],
            fill="red", arrow="last", arrowshape=(0.9 * self.r, self.r, self.r / 20))
        self.itemconfig(self.line, width=3)

        if halpin is None:
            halpin = "meter." + str(pyvcp_meter.n) + ".value"
            pyvcp_meter.n += 1
        self.halpin = halpin
        client.on_pin_change(halpin, self._on_value)

    def _on_value(self, val):
        alfa = self._value2angle(val)
        x = self.mid + 0.8 * self.r * math.cos(alfa)
        y = self.mid + 0.8 * self.r * math.sin(alfa)
        self.coords(self.line, self.mid, self.mid, x, y)

    def _value2angle(self, value):
        scale = (self.max_ - self.min_) / (self.max_alfa - self.min_alfa)
        alfa = self.min_alfa + (value - self.min_) / scale
        return max(self.min_alfa, min(self.max_alfa, alfa))

    def _draw_region(self, region):
        start, end, color = region
        start_a = -math.degrees(self._value2angle(start))
        end_a = -math.degrees(self._value2angle(end))
        extent = end_a - start_a
        halfwidth = math.floor(0.1 * self.r / 2) + 1
        xy = (self.pad + halfwidth, self.pad + halfwidth,
              self.size - self.pad - halfwidth, self.size - self.pad - halfwidth)
        self.create_arc(xy, start=start_a, extent=extent, outline=color,
                        width=(halfwidth - 1) * 2, style="arc")

    def _draw_ticks(self):
        value = self.min_
        while value <= self.max_:
            alfa = self._value2angle(value)
            x1, y1 = self.mid + self.r * math.cos(alfa), self.mid + self.r * math.sin(alfa)
            x2, y2 = self.mid + 0.85 * self.r * math.cos(alfa), self.mid + 0.85 * self.r * math.sin(alfa)
            xt, yt = self.mid + 0.75 * self.r * math.cos(alfa), self.mid + 0.75 * self.r * math.sin(alfa)
            self.create_text(xt, yt, font="Arial %d" % (self.size / 30 + 5), text="%g" % value)
            self.create_line(x1, y1, x2, y2, width=2)
            value += self.majorscale
        if self.minorscale > 0:
            value = self.min_
            while value <= self.max_:
                if (value % self.majorscale) != 0:
                    alfa = self._value2angle(value)
                    x1, y1 = self.mid + self.r * math.cos(alfa), self.mid + self.r * math.sin(alfa)
                    x2, y2 = self.mid + 0.9 * self.r * math.cos(alfa), self.mid + 0.9 * self.r * math.sin(alfa)
                    self.create_line(x1, y1, x2, y2)
                value += self.minorscale


class pyvcp_multilabel(Label):
    n = 0

    def __init__(self, master, client, halpin=None, disable_pin=False,
                 legends=[], initval=0, **kw):
        Label.__init__(self, master, **kw)
        if halpin is None:
            halpin = "multilabel." + str(pyvcp_multilabel.n)
            pyvcp_multilabel.n += 1
        self.legends = legends
        self.halpins = []
        for i, c in enumerate(legends):
            if i >= 6:
                break
            pin = halpin + ".legend" + str(i)
            self.halpins.append(pin)
            client.on_pin_change(pin, lambda val, idx=i: self._on_legend(idx, val))
        self.disable_pin = disable_pin
        if disable_pin:
            self.halpin_disable = halpin + ".disable"
            client.on_pin_change(self.halpin_disable, self._on_disable)
        # Initial display set from initval.
        if 0 <= initval < len(legends):
            Label.config(self, text=legends[initval])

    def _on_legend(self, idx, val):
        if val and idx < len(self.legends):
            Label.config(self, text=self.legends[idx])

    def _on_disable(self, val):
        Label.config(self, state=DISABLED if val else NORMAL)


class pyvcp_label(Label):
    n = 0

    def __init__(self, master, client, halpin=None, disable_pin=False, **kw):
        Label.__init__(self, master, **kw)
        if disable_pin:
            if halpin is None:
                halpin = "label." + str(pyvcp_label.n)
                pyvcp_label.n += 1
            self.halpin_disable = halpin + ".disable"
            client.on_pin_change(self.halpin_disable, self._on_disable)

    def _on_disable(self, val):
        Label.config(self, state=DISABLED if val else NORMAL)


class _pyvcp_image(Label):
    def __init__(self, master, client, images, halpin=None, **kw):
        Label.__init__(self, master, **kw)
        if isinstance(images, str):
            images = images.split()
        self.images = images
        if halpin is None:
            halpin = "number." + str(pyvcp_number.n)
            pyvcp_number.n += 1
        self.halpin = halpin
        client.on_pin_change(halpin, self._on_value)

    def _on_value(self, val):
        try:
            self.configure(image=self.images[val])
        except (IndexError, KeyError):
            print("Unknown image #%d on %s" % (val, self.halpin), file=sys.stderr)


class pyvcp_image_bit(_pyvcp_image):
    pass


class pyvcp_image_u32(_pyvcp_image):
    pass


# ============================================================
# Control widgets (OUT pins — widget → server on user interaction)
# ============================================================

class pyvcp_button(Button):
    n = 0

    def __init__(self, master, client, halpin=None, disable_pin=False, **kw):
        Button.__init__(self, master, **kw)
        if halpin is None:
            halpin = "button." + str(pyvcp_button.n)
            pyvcp_button.n += 1
        self.halpin = halpin
        self.client = client
        self.disable_pin = disable_pin
        self._disabled = False
        self.bind("<ButtonPress>", self._pressed)
        self.bind("<ButtonRelease>", self._released)
        if disable_pin:
            self.halpin_disable = halpin + ".disable"
            client.on_pin_change(self.halpin_disable, self._on_disable)

    def _pressed(self, event):
        if not self._disabled:
            self.client.set_pin(self.halpin, True)

    def _released(self, event):
        if not self._disabled:
            self.client.set_pin(self.halpin, False)

    def _on_disable(self, val):
        self._disabled = val
        Button.config(self, state=DISABLED if val else NORMAL)


class pyvcp_checkbutton(Checkbutton):
    n = 0

    def __init__(self, master, client, halpin=None, initval=0, **kw):
        self.v = BooleanVar(master)
        Checkbutton.__init__(self, master, variable=self.v, onvalue=1,
                             offvalue=0, command=self._on_toggle, **kw)
        if halpin is None:
            halpin = "checkbutton." + str(pyvcp_checkbutton.n)
        pyvcp_checkbutton.n += 1
        self.halpin = halpin
        self.client = client
        self._from_server = False

        # Server pushes the OUT pin state (initial sync).
        client.on_pin_change(halpin, self._on_server_value)
        # Changepin: server can toggle externally.
        self.changepin = halpin + ".changepin"
        client.on_pin_change(self.changepin, self._on_changepin)

    def _on_toggle(self):
        """User clicked the checkbox."""
        if not self._from_server:
            self.client.set_pin(self.halpin, self.v.get())

    def _on_server_value(self, val):
        """Server pushed the pin value (initial sync or echo)."""
        self._from_server = True
        self.v.set(val)
        self._from_server = False

    def _on_changepin(self, val):
        """External toggle from HAL."""
        if val:
            self._from_server = True
            self.v.set(not self.v.get())
            self._from_server = False
            self.client.set_pin(self.halpin, self.v.get())


class pyvcp_radiobutton(Frame):
    n = 0

    def __init__(self, master, client, halpin=None, initval=0, orient=None,
                 choices=[], **kw):
        Frame.__init__(self, master, bd=2, relief=GROOVE)
        self.client = client
        self.v = IntVar()
        self.v.set(1)
        self.choices = choices
        side = 'left' if orient else 'top'

        if halpin is None:
            halpin = "radiobutton." + str(pyvcp_radiobutton.n)
            pyvcp_radiobutton.n += 1

        self.halpins = []
        self._from_server = False
        for i, c in enumerate(choices):
            b = Radiobutton(self, text=str(c), variable=self.v,
                            value=pow(2, i), command=self._on_select)
            b.pack(side=side)
            if i == initval:
                b.select()
            pin = halpin + "." + str(c)
            self.halpins.append(pin)
            # Listen for server state on each choice pin.
            client.on_pin_change(pin, lambda val, idx=i: self._on_server_pin(idx, val))

    def _on_select(self):
        """User clicked a radio button."""
        if self._from_server:
            return
        index = int(math.log(self.v.get(), 2))
        for i, pin in enumerate(self.halpins):
            self.client.set_pin(pin, i == index)

    def _on_server_pin(self, idx, val):
        """Server pushed a pin value (initial sync)."""
        if val:
            self._from_server = True
            self.v.set(pow(2, idx))
            self._from_server = False


class pyvcp_scale(Scale):
    n = 0

    def __init__(self, master, client, resolution=1, halpin=None,
                 halparam=None, min_=0, max_=10, initval=0, param_pin=0, **kw):
        self.resolution = resolution
        Scale.__init__(self, master, resolution=self.resolution,
                       from_=min_, to=max_, command=self._on_change, **kw)
        if halpin is None:
            halpin = "scale." + str(pyvcp_scale.n)
        self.halpin = halpin
        self.client = client
        self._from_server = False
        self.param_pin = param_pin

        if param_pin:
            if halparam is None:
                halparam = "scale." + str(pyvcp_scale.n) + ".param_pin"
            self.halparam = halparam
            client.on_pin_change(halparam, self._on_param)

        pyvcp_scale.n += 1

        # Listen for server state on the OUT pins (initial sync).
        client.on_pin_change(halpin + "-f", self._on_server_value)

        self.bind('<Button-4>', self._wheel_up)
        self.bind('<Button-5>', self._wheel_down)

    def _on_change(self, value):
        """User dragged the slider."""
        if self._from_server:
            return
        self._send_value(float(value))

    def _on_server_value(self, val):
        """Server pushed the float pin value (initial sync)."""
        self._from_server = True
        self.set(val)
        self._from_server = False

    def _on_param(self, val):
        """External param_pin change from HAL."""
        self._from_server = True
        self.set(val)
        self._from_server = False

    def _wheel_up(self, event):
        val = self.get() + self.resolution
        self.set(val)
        self._send_value(val)

    def _wheel_down(self, event):
        val = self.get() - self.resolution
        self.set(val)
        self._send_value(val)

    def _send_value(self, val):
        """Send current scale value to server."""
        self.client.set_pin(self.halpin + "-f", float(val))
        self.client.set_pin(self.halpin + "-i", int(float(val)))


class pyvcp_spinbox(Spinbox):
    n = 0

    def __init__(self, master, client, halpin=None, halparam=None, param_pin=0,
                 min_=0, max_=100, initval=0, resolution=1, format="2.1f", **kw):
        self.v = DoubleVar()
        if 'increment' not in kw:
            kw['increment'] = resolution
        if 'from' not in kw:
            kw['from'] = min_
        if 'to' not in kw:
            kw['to'] = max_
        if 'format' not in kw:
            kw['format'] = "%" + format
        Spinbox.__init__(self, master, textvariable=self.v,
                         command=self._on_change, **kw)

        if halpin is None:
            halpin = "spinbox." + str(pyvcp_spinbox.n)
        self.halpin = halpin
        self.client = client
        self.min_ = min_
        self.max_ = max_
        self.resolution = resolution
        self.format_str = "%(b)" + format
        self._from_server = False
        self.param_pin = param_pin

        if param_pin:
            if halparam is None:
                halparam = "spinbox." + str(pyvcp_spinbox.n) + ".param_pin"
            self.halparam = halparam
            client.on_pin_change(halparam, self._on_param)

        pyvcp_spinbox.n += 1

        # Listen for server state (initial sync).
        client.on_pin_change(halpin, self._on_server_value)

        self.bind('<Button-4>', self._wheel_up)
        self.bind('<Button-5>', self._wheel_down)
        self.bind('<Return>', self._on_return)

    def _send_value(self):
        val = self.v.get()
        val = max(self.min_, min(self.max_, val))
        self.client.set_pin(self.halpin, val)

    def _on_change(self):
        if not self._from_server:
            self._send_value()

    def _on_return(self, event):
        if not self._from_server:
            self._send_value()

    def _on_server_value(self, val):
        self._from_server = True
        self.v.set(float(val))
        self._from_server = False

    def _on_param(self, val):
        self._from_server = True
        self.v.set(float(val))
        self._from_server = False

    def _wheel_up(self, event):
        val = self.v.get() + self.resolution
        if val <= self.max_:
            self.v.set(val)
            self._send_value()

    def _wheel_down(self, event):
        val = self.v.get() - self.resolution
        if val >= self.min_:
            self.v.set(val)
            self._send_value()


class pyvcp_dial(Canvas):
    n = 0

    def __init__(self, master, client, halpin=None, halparam=None, param_pin=0,
                 size=200, cpr=40, dialcolor="", edgecolor="", dotcolor="grey",
                 min_=None, max_=None, text=None, initval=0, resolution=0.1,
                 **kw):
        pad = size / 10
        self.counts = int(round(initval / resolution))
        self.out = self.counts * resolution
        self.origValue = initval
        self.funit = resolution
        self.origFunit = resolution
        self.mymin = min_
        self.mymax = max_

        Canvas.__init__(self, master, width=size, height=size)
        pad2 = pad - size / 15
        self.circle2 = self.create_oval(pad2, pad2, size - pad2, size - pad2, width=3)
        self.itemconfig(self.circle2, fill=edgecolor, activefill=edgecolor)
        self.circle = self.create_oval(pad, pad, size - pad, size - pad)
        self.itemconfig(self.circle, fill=dialcolor, activefill=dialcolor)
        self.mid = size / 2
        self.r = (size - 2 * pad) / 2
        self.alfa = 0
        self.d_alfa = 2 * math.pi / cpr
        self.size = size
        self.dotcolor = dotcolor

        self.dot = self.create_oval(self._dot_coords())
        self.itemconfig(self.dot, fill=dotcolor, activefill="black")
        self.line = self.create_line(
            self.mid + self.r * math.cos(self.alfa),
            self.mid + self.r * math.sin(self.alfa),
            self.mid + self.r * 1.1 * math.cos(self.alfa),
            self.mid + self.r * 1.1 * math.sin(self.alfa))
        self.itemconfig(self.line, arrow="last", arrowshape=(10, 10, 10), width=10)

        self.txtroom = int(size / 6)
        if text is not None:
            self.create_text([self.mid, self.mid - self.txtroom],
                             text=text, font=('Arial', -self.txtroom))
        self.dro = self.create_text([self.mid, self.mid], font=('Arial', -self.txtroom))
        self._update_dro()
        self.delta = self.create_text([self.mid, self.mid + self.txtroom],
                                      text='x ' + str(self.funit),
                                      font=('Arial', -self.txtroom))

        self.bind('<Button-4>', self._wheel_up)
        self.bind('<Button-5>', self._wheel_down)
        self.bind('<Button1-Motion>', self._motion)
        self.bind('<ButtonPress>', self._bdown)
        self.bind('<ButtonRelease>', self._bup)
        self.bind('<Double-1>', self._scale_dn)
        self.bind('<Double-2>', self._scale_reset)
        self.bind('<Double-3>', self._scale_up)
        self.bind('<Shift-1>', self._reset_value)

        self._draw_ticks(cpr)
        self.dragstart = 0

        if halpin is None:
            halpin = "dial." + str(pyvcp_dial.n) + ".out"
        self.halpin = halpin
        self.client = client
        self._from_server = False

        if halparam is None:
            halparam = "dial." + str(pyvcp_dial.n) + ".param_pin"
        self.halparam = halparam
        pyvcp_dial.n += 1

        # Server state: sync from OUT pin and watch param_pin.
        client.on_pin_change(halpin, self._on_server_value)
        client.on_pin_change(halparam, self._on_param)

    def _send(self):
        self.client.set_pin(self.halpin, self.out)

    def _on_server_value(self, val):
        """Initial sync of the output pin from server."""
        self._from_server = True
        self.out = val
        self.counts = int(round(val / self.funit))
        self._update_dro()
        self._update_dot()
        self._from_server = False

    def _on_param(self, val):
        """External param_pin change."""
        self._from_server = True
        self.out = val
        self.counts = int(round(val / self.funit))
        self._update_dro()
        self._update_dot()
        self._from_server = False

    def _wheel_up(self, event):
        self._up()

    def _wheel_down(self, event):
        self._down()

    def _bdown(self, event):
        self.dragstart = math.atan2(event.y - self.mid, event.x - self.mid)
        self.itemconfig(self.dot, fill="black", activefill="black")

    def _bup(self, event):
        self.itemconfig(self.dot, fill=self.dotcolor)

    def _motion(self, event):
        dragstop = math.atan2(event.y - self.mid, event.x - self.mid)
        delta = dragstop - self.dragstart
        if delta >= self.d_alfa:
            self._up()
            self.dragstart = math.atan2(event.y - self.mid, event.x - self.mid)
        elif delta <= -self.d_alfa:
            self._down()
            self.dragstart = math.atan2(event.y - self.mid, event.x - self.mid)
        self.itemconfig(self.dot, fill="black", activefill="black")

    def _up(self):
        self.alfa += self.d_alfa
        self.counts += 1
        self.out = self.counts * self.funit
        if self.mymax is not None and self.out > self.mymax:
            self.out = self.mymax
            self.counts = int(round(self.mymax / self.funit))
        self._update_dot()
        self._update_dro()
        self._send()

    def _down(self):
        self.alfa -= self.d_alfa
        self.counts -= 1
        self.out = self.counts * self.funit
        if self.mymin is not None and self.out < self.mymin:
            self.out = self.mymin
            self.counts = int(round(self.mymin / self.funit))
        self._update_dot()
        self._update_dro()
        self._send()

    def _scale_dn(self, event):
        self.funit /= 10.0
        self.counts *= 10
        self._update_scale()
        self._update_dro()

    def _scale_up(self, event):
        self.funit *= 10.0
        self.counts = (self.counts + 5) // 10
        self.out = self.counts * self.funit
        self._update_scale()
        self._update_dro()

    def _scale_reset(self, event):
        self.funit = self.origFunit
        self.counts = int(round(self.out / self.funit))
        self.out = self.counts * self.funit
        self._update_scale()

    def _reset_value(self, event):
        self.counts = int(round(self.origValue / self.funit))
        self.out = self.counts * self.funit
        self._update_dot()
        self._update_dro()
        self._send()

    def _dot_coords(self):
        DOTR = 0.04 * self.size
        DOTPOS = 0.85
        midx = self.mid + DOTPOS * self.r * math.cos(self.alfa)
        midy = self.mid + DOTPOS * self.r * math.sin(self.alfa)
        return midx - DOTR, midy - DOTR, midx + DOTR, midy + DOTR

    def _update_dot(self):
        self.coords(self.dot, self._dot_coords())
        self.coords(self.line,
                    self.mid + self.r * math.cos(self.alfa),
                    self.mid + self.r * math.sin(self.alfa),
                    self.mid + self.r * 1.1 * math.cos(self.alfa),
                    self.mid + self.r * 1.1 * math.sin(self.alfa))

    def _update_dro(self):
        decimals = max(0, len(str(self.funit)) - 2)
        valtext = "{:.{}f}".format(self.out, decimals)
        self.itemconfig(self.dro, text=valtext)

    def _update_scale(self):
        self.itemconfig(self.delta, text='x ' + str(self.funit))

    def _draw_ticks(self, cpr):
        for n in range(0, cpr, 2):
            for i in range(2):
                startx = self.mid + self.r * math.cos((n + i) * self.d_alfa)
                starty = self.mid + self.r * math.sin((n + i) * self.d_alfa)
                length = 1.15 if i == 0 else 1.1
                width = 2 if i == 0 else 1
                stopx = self.mid + length * self.r * math.cos((n + i) * self.d_alfa)
                stopy = self.mid + length * self.r * math.sin((n + i) * self.d_alfa)
                self.create_line(startx, starty, stopx, stopy, width=width)


class pyvcp_jogwheel(Canvas):
    n = 0

    def __init__(self, master, client, halpin=None, text=None, clear_pin=0,
                 scale_pin=0, fillcolor="lightgrey", bgcolor="lightgrey",
                 size=200, cpr=40, **kw):
        pad = size / 10
        self.count = 0
        self.scale = 0.0
        self.drotxt = 0.0
        Canvas.__init__(self, master, width=size, height=size)
        pad2 = pad - size / 15
        self.create_oval(pad2, pad2, size - pad2, size - pad2, width=3)
        self.circle = self.create_oval(pad, pad, size - pad, size - pad)
        self.itemconfig(self.circle, fill=bgcolor, activefill=fillcolor)
        self.mid = size / 2
        self.r = (size - 2 * pad) / 2
        self.alfa = 0
        self.d_alfa = 2 * math.pi / cpr
        self.size = size
        self.client = client
        self._has_clear = bool(clear_pin)
        self._has_scale = bool(scale_pin)

        self.dot = self.create_oval(self._dot_coords())
        self.itemconfig(self.dot, fill="black")
        self.line = self.create_line(
            self.mid + self.r * math.cos(self.alfa),
            self.mid + self.r * math.sin(self.alfa),
            self.mid + self.r * 1.1 * math.cos(self.alfa),
            self.mid + self.r * 1.1 * math.sin(self.alfa))
        self.itemconfig(self.line, arrow="last", arrowshape=(10, 10, 10), width=8)

        self.txtroom = int(size / 10)
        if text is not None:
            self.create_text([self.mid, self.mid - self.txtroom],
                             text=text, font=('Arial', -self.txtroom))
        if self._has_clear:
            self.dro_item = self.create_text([self.mid, self.mid],
                                             text="0.0000", font=('Arial', -self.txtroom))
        if self._has_scale:
            self.scale_item = self.create_text([self.mid, self.mid + self.txtroom],
                                               text='x 0.0', font=('Arial', -self.txtroom))

        self.bind('<Button-4>', self._wheel_up)
        self.bind('<Button-5>', self._wheel_down)
        self.bind('<Button1-Motion>', self._motion)
        self.bind('<ButtonPress>', self._bdown)
        self.bind('<Shift-1>', self._reset_value)
        self._draw_ticks(cpr)
        self.dragstart = 0

        # Pin setup.
        name = ""
        if halpin is None:
            name = ".count"
            halpin = "jogwheel." + str(pyvcp_jogwheel.n) + name
            pyvcp_jogwheel.n += 1
        self.halpin = halpin
        base = halpin[:-6] if name else halpin

        # Listen for server state on count pin (initial sync).
        client.on_pin_change(halpin, self._on_server_count)

        if self._has_clear:
            self.clear_pin_name = base + ".reset"
            client.on_pin_change(self.clear_pin_name, self._on_clear)
        if self._has_scale:
            self.scale_pin_name = base + ".scale"
            client.on_pin_change(self.scale_pin_name, self._on_scale)

    def _send(self):
        self.client.set_pin(self.halpin, float(self.count))

    def _on_server_count(self, val):
        """Initial sync of count from server."""
        self.count = int(val)

    def _on_clear(self, val):
        if val:
            self.drotxt = 0.0
            self.itemconfig(self.dro_item, text="0.0000")

    def _on_scale(self, val):
        self.scale = val
        if self._has_scale:
            self.itemconfig(self.scale_item, text='x ' + str(val))

    def _reset_value(self, event):
        self.drotxt = 0.0
        if self._has_clear:
            self.itemconfig(self.dro_item, text="0.0000")

    def _up(self):
        self.alfa += self.d_alfa
        self.count += 1
        self.drotxt += self.scale
        self._update_dot()
        self._update_dro()
        self._send()

    def _down(self):
        self.alfa -= self.d_alfa
        self.count -= 1
        self.drotxt -= self.scale
        self._update_dot()
        self._update_dro()
        self._send()

    def _wheel_up(self, event):
        self._up()

    def _wheel_down(self, event):
        self._down()

    def _bdown(self, event):
        self.dragstart = math.atan2(event.y - self.mid, event.x - self.mid)

    def _motion(self, event):
        dragstop = math.atan2(event.y - self.mid, event.x - self.mid)
        delta = dragstop - self.dragstart
        if delta >= self.d_alfa:
            self._up()
            self.dragstart = math.atan2(event.y - self.mid, event.x - self.mid)
        elif delta <= -self.d_alfa:
            self._down()
            self.dragstart = math.atan2(event.y - self.mid, event.x - self.mid)

    def _dot_coords(self):
        DOTR = 0.06 * self.size
        DOTPOS = 0.85
        midx = self.mid + DOTPOS * self.r * math.cos(self.alfa)
        midy = self.mid + DOTPOS * self.r * math.sin(self.alfa)
        return midx - DOTR, midy - DOTR, midx + DOTR, midy + DOTR

    def _update_dot(self):
        self.coords(self.dot, self._dot_coords())
        self.coords(self.line,
                    self.mid + self.r * math.cos(self.alfa),
                    self.mid + self.r * math.sin(self.alfa),
                    self.mid + self.r * 1.1 * math.cos(self.alfa),
                    self.mid + self.r * 1.1 * math.sin(self.alfa))

    def _update_dro(self):
        if self._has_clear:
            self.itemconfig(self.dro_item, text='{:.4f}'.format(self.drotxt))

    def _draw_ticks(self, cpr):
        for n in range(0, cpr):
            startx = self.mid + self.r * math.cos(n * self.d_alfa)
            starty = self.mid + self.r * math.sin(n * self.d_alfa)
            stopx = self.mid + 1.15 * self.r * math.cos(n * self.d_alfa)
            stopy = self.mid + 1.15 * self.r * math.sin(n * self.d_alfa)
            self.create_line([startx, starty, stopx, stopy])


# ============================================================
# Layout widgets (no pins)
# ============================================================

class pyvcp_vbox(Frame):
    def __init__(self, master, client, bd=0, relief=FLAT):
        Frame.__init__(self, master, bd=bd, relief=relief)
        self.fill = 'x'
        self.side = 'top'
        self.anchor = 'center'
        self.expand = 'yes'

    def add(self, container, widget):
        if isinstance(widget, pyvcp_boxexpand):
            self.expand = widget.expand
            return
        if isinstance(widget, pyvcp_boxfill):
            self.fill = widget.fill
            return
        if isinstance(widget, pyvcp_boxanchor):
            self.anchor = widget.anchor
            return
        widget.pack(side=self.side, anchor=self.anchor, fill=self.fill, expand=self.expand)


class pyvcp_hbox(Frame):
    def __init__(self, master, client, bd=0, relief=FLAT):
        Frame.__init__(self, master, bd=bd, relief=relief)
        self.fill = 'y'
        self.side = 'left'
        self.anchor = 'center'
        self.expand = 'yes'

    def add(self, container, widget):
        if isinstance(widget, pyvcp_boxexpand):
            self.expand = widget.expand
            return
        if isinstance(widget, pyvcp_boxfill):
            self.fill = widget.fill
            return
        if isinstance(widget, pyvcp_boxanchor):
            self.anchor = widget.anchor
            return
        widget.pack(side=self.side, anchor=self.anchor, fill=self.fill)


class pyvcp_boxfill:
    def __init__(self, master, client, fill):
        self.fill = fill


class pyvcp_boxanchor:
    def __init__(self, master, client, anchor):
        self.anchor = anchor


class pyvcp_boxexpand:
    def __init__(self, master, client, expand):
        self.expand = expand


class pyvcp_labelframe(LabelFrame):
    def __init__(self, master, client, **kw):
        LabelFrame.__init__(self, master, **kw)
        self.pack(expand=1, fill=BOTH)

    def add(self, container, widget):
        widget.pack(side="top", fill="both", expand="yes")


class pyvcp_tabs(bwidget.NoteBook):
    def __init__(self, master, client, cnf={}, **kw):
        self.names = kw.pop("names", [])
        self.idx = 0
        self._require(master)
        Widget.__init__(self, master, "NoteBook", cnf, kw)

    def add(self, container, child):
        child.pack(side="top", fill="both", anchor="ne")
        if self.idx == 1:
            self.raise_page(self.names[0])

    def getcontainer(self):
        if len(self.names) < self.idx:
            self.names.append("Tab-%d" % self.idx)
        name = self.names[self.idx]
        self.idx += 1
        return self.insert("end", name, text=name)


class pyvcp_table(Frame):
    def __init__(self, master, client, flexible_rows=[], flexible_columns=[],
                 uniform_columns="", uniform_rows="", **kw):
        Frame.__init__(self, master, **kw)
        for r in flexible_rows:
            self.grid_rowconfigure(r, weight=1)
        for c in flexible_columns:
            self.grid_columnconfigure(c, weight=1)
        for i, r in enumerate(uniform_rows):
            self.grid_rowconfigure(i + 1, uniform=r)
        for i, c in enumerate(uniform_columns):
            self.grid_columnconfigure(i + 1, uniform=c)
        self._r = self._c = 0
        self.occupied = {}
        self.span = (1, 1)
        self.sticky = "ne"

    def add(self, container, child):
        if isinstance(child, pyvcp_tablerow):
            self._r += 1
            self._c = 1
            return
        elif isinstance(child, pyvcp_tablespan):
            self.span = child.span
            return
        elif isinstance(child, pyvcp_tablesticky):
            self.sticky = child.sticky
            return
        r, c = self._r, self._c
        while (r, c) in self.occupied:
            c += 1
        rs, cs = self.span
        child.grid(row=r, column=c, rowspan=rs, columnspan=cs, sticky=self.sticky)
        for ri in range(r, r + rs):
            for ci in range(c, c + cs):
                self.occupied[ri, ci] = True
        self.span = 1, 1
        self._c = c + cs


class pyvcp_tablerow:
    def __init__(self, master, client):
        pass


class pyvcp_tablespan:
    def __init__(self, master, client, rows=1, columns=1):
        self.span = rows, columns


class pyvcp_tablesticky:
    def __init__(self, master, client, sticky):
        self.sticky = sticky


class pyvcp_include(Frame):
    def __init__(self, master, client, src, expand="yes", fill="both",
                 anchor="center", prefix=None, **kw):
        Frame.__init__(self, master, **kw)
        self.fill = fill
        self.anchor = anchor
        self.expand = expand
        # Parse included XML.
        import vcpparse
        import xml.dom.minidom
        try:
            doc = xml.dom.minidom.parse(src)
        except Exception as detail:
            print("Error: could not open", src, "!")
            print(detail)
            sys.exit(1)
        for e in doc.childNodes:
            if e.nodeType == e.ELEMENT_NODE and e.localName == "pyvcp":
                break
        if e.localName != "pyvcp":
            print("Error: no pyvcp element in file!")
            sys.exit()
        vcpparse.nodeiterator(e, self, client)

    def add(self, container, widget):
        widget.pack(fill=self.fill, anchor=self.anchor, expand=self.expand)


class _pyvcp_dummy:
    def add(self, container, widget):
        pass

    def pack(self, *args, **kw):
        pass


class pyvcp_title(_pyvcp_dummy):
    def __init__(self, master, client, title, iconname=None):
        master.wm_title(title)
        if iconname:
            master.wm_iconname(iconname)


class pyvcp_axisoptions(_pyvcp_dummy):
    def __init__(self, master, client):
        import rs274.options
        rs274.options.install(master)


class pyvcp_option(_pyvcp_dummy):
    def __init__(self, master, client, pattern, value, priority=None):
        master.option_add(pattern, value, priority)


class pyvcp_image(_pyvcp_dummy):
    all_images = {}

    def __init__(self, master, client, name, **kw):
        self.all_images[name] = PhotoImage(name, kw, master)


# Build elements list (same pattern as pyvcp_widgets.py).
elements = []
__all__ = []
for _key in list(globals().keys()):
    if _key.startswith("pyvcp_"):
        elements.append(_key[6:])
        __all__.append(_key)
