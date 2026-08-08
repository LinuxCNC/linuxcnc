#!/usr/bin/env python3
"""
GTK Mesa tests screen

Embedded panel for PC and Mesa tests by zz912

Based on the design of Mesa Configuration Tool II
https://github.com/jethornton/mesact
Copyright (c) 2022 jethornton

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any laforter version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
"""

import gi
gi.require_version('Gtk', '3.0')
gi.require_version('Gdk', '3.0')
from gi.repository import Gtk
from gi.repository import Gdk

import subprocess  # Used to run external system commands and capture their output
from subprocess import Popen, PIPE


class MesaTests:
    def __init__(self, halcomp,builder,useropts):
        self.builder = builder
        self.password = None  # cache for sudo password

        textview_servo = self.builder.get_object("textview_servo")
        buffer_servo = textview_servo.get_buffer()
        buffer_servo.connect("changed", self.on_textbuffer_changed)

        textview_nic = self.builder.get_object("textview_nic")
        buffer_nic = textview_nic.get_buffer()
        buffer_nic.connect("changed", self.on_textbuffer_changed)


    def ask_sudo_password(self, gtkbutton):
        """
        Show a simple dialog asking for the sudo password.
        Returns the password string or None if cancelled.
        """
        parent = gtkbutton.get_toplevel()

        dialog = Gtk.Dialog(
            title="sudo password",
            transient_for=parent,
            modal=True,
        )

        # Set dialog default size (width x height)
        dialog.set_default_size(300, 100)

        # Center dialog buttons horizontally
        action_area = dialog.get_action_area()
        action_area.set_halign(Gtk.Align.CENTER)

        # Add dialog buttons
        dialog.add_button("_OK", Gtk.ResponseType.OK)
        dialog.add_button("_Cancel", Gtk.ResponseType.CANCEL)

        # Get the dialog content area
        box = dialog.get_content_area()
        box.set_spacing(10)

        # Create UI elements
        label = Gtk.Label(label="Please enter sudo password:")
        label.set_halign(Gtk.Align.START)      # Align label to the left
        entry = Gtk.Entry()
        entry.set_visibility(False)            # Hide text (password mode)
        entry.set_activates_default(True)      # Enter key triggers default button

        # Pack UI elements into the dialog
        box.add(label)
        box.add(entry)
        box.show_all()

        dialog.set_default_response(Gtk.ResponseType.OK)
        response = dialog.run()

        password = None
        if response == Gtk.ResponseType.OK:
            password = entry.get_text()

        dialog.destroy()
        return password or None

    def on_btn_ip_nic_released(self, gtkbutton):
        ip = subprocess.check_output(['ip', '-br', 'addr', 'show'], text=True)

        textview = self.builder.get_object("textview_ip_info")
        buffer = textview.get_buffer()
        buffer.set_text(ip)
        
    def on_btn_mb_info_released(self, gtkbutton):
        # Get / cache sudo password
        if not self.password:
            self.password = self.ask_sudo_password(gtkbutton)

        # Get the TextView for output
        textview = self.builder.get_object("textview_pc_info")
        buffer = textview.get_buffer()

        if self.password is None:
            buffer.set_text("Operation cancelled. No password provided.")
            return

        try:
            # Run dmidecode -t 2 with sudo
            p = Popen(
                ['sudo', '-S', 'dmidecode', '-t', '2'],
                stdin=PIPE,
                stdout=PIPE,
                stderr=PIPE,
                text=True,
            )

            stdout, stderr = p.communicate(self.password + '\n')

            if p.returncode == 0:
                output = stdout
            else:
                output = stderr

            result_text = f"Return Code: {p.returncode}\n{output}"
            buffer.set_text(result_text)

        except Exception as e:
            buffer.set_text(f"Error running dmidecode:\n{e}")        

    def on_btn_cpu_info_released(self, gtkbutton):
        result = subprocess.check_output("lscpu",shell=True, text=True) 

        textview = self.builder.get_object("textview_pc_info")
        buffer = textview.get_buffer()
        buffer.set_text(result)

    def on_btn_nic_info_released(self, gtkbutton):
        result = subprocess.check_output("lspci | grep -i 'ethernet'",shell=True, text=True)

        textview = self.builder.get_object("textview_pc_info")
        buffer = textview.get_buffer()
        buffer.set_text(result)
        
    def on_btn_copy_released(self, gtkbutton):
        """
        Copy the content of textview_pc_info to the system clipboard
        and replace the content with a confirmation message.
        """
        # Get the TextView and its buffer
        textview = self.builder.get_object("textview_pc_info")
        buffer = textview.get_buffer()

        # Read current text from the buffer
        start_iter = buffer.get_start_iter()
        end_iter = buffer.get_end_iter()
        text = buffer.get_text(start_iter, end_iter, True)

        # Access the system clipboard
        clipboard = Gtk.Clipboard.get(Gdk.SELECTION_CLIPBOARD)

        # Set clipboard text
        clipboard.set_text(text, -1)

        # Replace TextView content with notification message
        buffer.set_text("Output copied to clipboard")

    def on_btn_servo_thread_tmax_released(self, gtkbutton):
        result = subprocess.check_output("halcmd show param servo-thread.tmax",shell=True, text=True)

        # Remove empty lines
        result = "\n".join(
            line for line in result.splitlines()
            if line.strip()  # keeps only non-empty lines
        )

        textview = self.builder.get_object("textview_servo")
        buffer = textview.get_buffer()
        
        if buffer.get_char_count() > 0:
            buffer.insert(buffer.get_end_iter(), "\n\n")
        buffer.insert(buffer.get_end_iter(), result)

        value = None
        for line in result.splitlines():
            line = line.strip()
            if line.endswith("servo-thread.tmax"):
                parts = line.split()
                value = int(parts[3])  # 4. column is Value
                break

        spin_thread_tmax = self.builder.get_object("sbtn_servo_thread_tmax")
        spin_thread_tmax.set_value(value)

    def on_btn_servo_thread_period_released(self, combo):
        result = subprocess.check_output("halcmd show thread servo-thread",shell=True, text=True)

        # Remove empty lines
        result = "\n".join(
            line for line in result.splitlines()
            if line.strip()  # keeps only non-empty lines
        )

        textview = self.builder.get_object("textview_servo")
        buffer = textview.get_buffer()
        
        if buffer.get_char_count() > 0:
            buffer.insert(buffer.get_end_iter(), "\n\n")
        buffer.insert(buffer.get_end_iter(), result)

        value = None
        for line in result.splitlines():
            line = line.strip()
            if "servo-thread" in line:
                parts = line.split()
                value = int(parts[0])  # 1. column is Value
                break

        spin_thread_period = self.builder.get_object("sbtn_servo_thread_period")
        spin_thread_period.set_value(value)

    def on_btn_calc_servo_released(self, gtkbutton):
        # Get UI objects
        textview = self.builder.get_object("textview_servo")
        buffer = textview.get_buffer()

        spin_thread_tmax = self.builder.get_object("sbtn_servo_thread_tmax")
        t_max = spin_thread_tmax.get_value()

        spin_thread_period = self.builder.get_object("sbtn_servo_thread_period")
        period = spin_thread_period.get_value()

        label_result = self.builder.get_object("lbl_result_servo")

        # Conditions
        if t_max <= 0:
            if buffer.get_char_count() > 0:
                buffer.insert(buffer.get_end_iter(), "\n\n")
            buffer.insert(buffer.get_end_iter(), "Servo Thread tmax must be greater than 0")
            return

        if period <= 0:
            if buffer.get_char_count() > 0:
                buffer.insert(buffer.get_end_iter(), "\n\n")
            buffer.insert(buffer.get_end_iter(), "Servo Thread period must be greater than 0")
            return

        # Calculation
        result = t_max / period * 100
        label_result.set_text(f'{result:.0f}%')


    def on_btn_read_tmax_released(self, gtkbutton):
        result = subprocess.check_output("halcmd show param hm2*read.tmax",shell=True, text=True)

        # Remove empty lines
        result = "\n".join(
            line for line in result.splitlines()
            if line.strip()  # keeps only non-empty lines
        )

        textview = self.builder.get_object("textview_nic")
        buffer = textview.get_buffer()
        
        if buffer.get_char_count() > 0:
            buffer.insert(buffer.get_end_iter(), "\n\n")
        buffer.insert(buffer.get_end_iter(), result)

        value = None
        for line in result.splitlines():
            line = line.strip()
            if "read.tmax" in line:
                parts = line.split()
                value = int(parts[3])  # 4. column is Value
                break

        spin_thread_period = self.builder.get_object("sbtn_read_tmax")
        spin_thread_period.set_value(value)

    def on_btn_write_tmax_released(self, gtkbutton):
        result = subprocess.check_output("halcmd show param hm2*write.tmax",shell=True, text=True)

        # Remove empty lines
        result = "\n".join(
            line for line in result.splitlines()
            if line.strip()  # keeps only non-empty lines
        )

        textview = self.builder.get_object("textview_nic")
        buffer = textview.get_buffer()
        
        if buffer.get_char_count() > 0:
            buffer.insert(buffer.get_end_iter(), "\n\n")
        buffer.insert(buffer.get_end_iter(), result)

        value = None
        for line in result.splitlines():
            line = line.strip()
            if "write.tmax" in line:
                parts = line.split()
                value = int(parts[3])  # 4. column is Value
                break

        spin_thread_period = self.builder.get_object("sbtn_write_tmax")
        spin_thread_period.set_value(value)

    def on_btn_servo_thread_period_nic_released(self, combo):
        result = subprocess.check_output("halcmd show thread servo-thread",shell=True, text=True)

        # Remove empty lines
        result = "\n".join(
            line for line in result.splitlines()
            if line.strip()  # keeps only non-empty lines
        )

        textview = self.builder.get_object("textview_nic")
        buffer = textview.get_buffer()
        
        if buffer.get_char_count() > 0:
            buffer.insert(buffer.get_end_iter(), "\n\n")
        buffer.insert(buffer.get_end_iter(), result)

        value = None
        for line in result.splitlines():
            line = line.strip()
            if "servo-thread" in line:
                parts = line.split()
                value = int(parts[0])  # 1. column is Value
                break

        spin_thread_period = self.builder.get_object("sbtn_servo_thread_period_nic")
        spin_thread_period.set_value(value)

    def on_btn_nic_calculate_released(self, gtkbutton):
        # Get UI objects
        textview = self.builder.get_object("textview_nic")
        buffer = textview.get_buffer()

        spin_read_tmax = self.builder.get_object("sbtn_read_tmax")
        read_tmax = spin_read_tmax.get_value()

        spin_write_tmax = self.builder.get_object("sbtn_write_tmax")
        write_tmax = spin_write_tmax.get_value()

        spin_thread_period = self.builder.get_object("sbtn_servo_thread_period_nic")
        period = spin_thread_period.get_value()

        label_result = self.builder.get_object("lbl_result_nic")

        # Conditions
        if read_tmax <= 0:
            if buffer.get_char_count() > 0:
                buffer.insert(buffer.get_end_iter(), "\n\n")
            buffer.insert(buffer.get_end_iter(), "Parameter read.tmax must be greater than 0")
            return

        if write_tmax <= 0:
            if buffer.get_char_count() > 0:
                buffer.insert(buffer.get_end_iter(), "\n\n")
            buffer.insert(buffer.get_end_iter(), "Parameter write.tmax must be greater than 0")
            return

        if period <= 0:
            if buffer.get_char_count() > 0:
                buffer.insert(buffer.get_end_iter(), "\n\n")
            buffer.insert(buffer.get_end_iter(), "Servo Thread period must be greater than 0")
            return

        # Calculation
        rw_tmax = read_tmax + write_tmax
        result = rw_tmax / period * 100
        label_result.set_text(f'{result:.0f}%')

    def on_btn_packet_error_released(self, gtkbutton):
        result = subprocess.check_output("halcmd show pin hm2*packet-error-total",shell=True, text=True)

        # Remove empty lines
        result = "\n".join(
            line for line in result.splitlines()
            if line.strip()  # keeps only non-empty lines
        )

        textview = self.builder.get_object("textview_nic")
        buffer = textview.get_buffer()
        
        if buffer.get_char_count() > 0:
            buffer.insert(buffer.get_end_iter(), "\n\n")
        buffer.insert(buffer.get_end_iter(), result)

        value = None
        for line in result.splitlines():
            line = line.strip()
            if "packet-error-total" in line:
                parts = line.split()
                value = int(parts[3],16)  # 4. column is Value
                break

        label_packet = self.builder.get_object("lbl_packet_error")
        label_packet.set_text(str(value))

    # Automatic scroll for both text views
    def on_textbuffer_changed(self, buffer):
        # Get both textviews and their buffers
        textview_servo = self.builder.get_object("textview_servo")
        buffer_servo = textview_servo.get_buffer()

        textview_nic = self.builder.get_object("textview_nic")
        buffer_nic = textview_nic.get_buffer()

        # Decide which textview this buffer belongs to
        if buffer is buffer_servo:
            textview = textview_servo
            attr_name = "end_mark_servo"
        elif buffer is buffer_nic:
            textview = textview_nic
            attr_name = "end_mark_nic"
        else:
            # Unknown buffer, do nothing
            return

        end_iter = buffer.get_end_iter()

        # Get existing mark or create a new one lazily
        mark = getattr(self, attr_name, None)
        if mark is None:
            mark = buffer.create_mark(attr_name, end_iter, False)
            setattr(self, attr_name, mark)

        # Move mark to the end
        buffer.move_mark(mark, end_iter)

        # Scroll to the bottom
        textview.scroll_to_mark(
            mark,
            0.0,   # within_margin
            True,  # use_align
            0.0,   # xalign
            1.0    # yalign
        )

def get_handlers(halcomp,builder,useropts):
    return [MesaTests(halcomp,builder,useropts)]
