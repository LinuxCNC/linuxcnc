#!/usr/bin/python2
# coding=utf-8

import os
import sys
import time
import subprocess
import threading
import socket
import argparse
from six.moves import configparser

from machinekit import service
from machinekit import config


MJPG_STREAMER_PLUGIN_PATH = '/usr/local/lib/mjpg-streamer/'


class VideoDevice(object):
    process = None
    service = None
    txtRecord = None
    sdname = ''
    framerate = 30
    resolution = '640x480'
    quality = 80
    device = '/dev/video0'
    buffer_size = 1
    port = 0
    dsname = ''
    zmq_uri = ''
    arguments = ''


class VideoServer(threading.Thread):
    def __init__(self, inifile, host='', loopback=False, svc_uuid=None, debug=False):
        threading.Thread.__init__(self)
        self.inifile = inifile
        self.host = host
        self.loopback = loopback
        self.svc_uuid = svc_uuid
        self.debug = debug

        self.videoDevices = {}
        self.cfg = configparser.ConfigParser(defaults={'arguments': ''})
        self.cfg.read(self.inifile)
        if self.debug:
            print("video devices:", self.cfg.sections())
        for n in self.cfg.sections():
            video_device = VideoDevice()

            video_device.framerate = self.cfg.getint(n, 'framerate')
            video_device.resolution = self.cfg.get(n, 'resolution')
            video_device.quality = self.cfg.get(n, 'quality')
            video_device.device = self.cfg.get(n, 'device')
            video_device.buffer_size = self.cfg.getint(n, 'bufferSize')
            video_device.arguments = self.cfg.get(n, 'arguments')
            self.videoDevices[n] = video_device
            if self.debug:
                print("framerate:", video_device.framerate)
                print("resolution:", video_device.resolution)
                print("quality:", video_device.quality)
                print("device:", video_device.device)
                print("bufferSize:", video_device.buffer_size)
                print("arguments:", video_device.arguments)

    def start_video(self, device_id):
        video_device = self.videoDevices[device_id]

        if video_device.process is not None:
            print("video device already running")
            return

        sock = socket.socket()
        sock.bind(('', 0))
        port = sock.getsockname()[1]
        sock.close()

        base_uri = 'tcp://'
        if self.loopback:
            base_uri += '127.0.0.1'
        else:
            base_uri += '*'

        video_device.port = port
        video_device.zmq_uri = '%s:%i' % (base_uri, video_device.port)
        video_device.dsname = video_device.zmq_uri.replace('*', self.host)

        if self.debug:
            print("dsname = ", video_device.dsname, "port =", video_device.port)

        libpath = MJPG_STREAMER_PLUGIN_PATH
        os.environ['LD_LIBRARY_PATH'] = libpath

        arguments = ""
        if video_device.arguments is not '':
            arguments = ' ' + video_device.arguments

        command = [
            'mjpg_streamer -i "{libpath}input_uvc.so -n -f {framerate} -r {resolution} -q {quality} -d {device}" '
            '-o " {libpath}output_zmqserver.so --address {uri} --buffer_size {buffer_size}"{args}'.format(
                libpath=libpath,
                framerate=video_device.framerate,
                resolution=video_device.resolution,
                quality=video_device.quality,
                device=video_device.device,
                uri=video_device.zmq_uri,
                buffer_size=video_device.buffer_size,
                args=arguments,
            )
        ]

        if self.debug:
            print("command:", command)

        video_device.process = subprocess.Popen(command, shell=True)

        try:
            video_device.service = service.Service(
                type_='video',
                svc_uuid=self.svc_uuid,
                dsn=video_device.dsname,
                port=video_device.port,
                host=self.host,
                loopback=self.loopback,
                debug=self.debug,
            )
            video_device.service.publish()
        except Exception as e:
            print('cannot register DNS service', e)

    def stop_video(self, device_id):
        video_device = self.videoDevices[device_id]

        if video_device.process is None:
            print("video device not running")
            return

        video_device.service.unpublish()
        video_device.process.terminate()
        video_device.process = None
        video_device.service = None

    def run(self):
        if self.debug:
            print("run called")

        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            for n in self.videoDevices:
                video_device = self.videoDevices[n]
                if video_device.process is None:
                    continue
                self.stop_video(n)


def main():
    parser = argparse.ArgumentParser(
        description='Videoserver provides a webcam interface for Machinetalk'
    )
    parser.add_argument('-i', '--ini', help='INI file', default='video.ini')
    parser.add_argument('-d', '--debug', help='Enable debug mode', action='store_true')
    parser.add_argument('webcams', help='List of webcams to stream', nargs='+')

    args = parser.parse_args()

    debug = args.debug

    mkconfig = config.Config()
    mkini = os.getenv("MACHINEKIT_INI")
    if mkini is None:
        mkini = mkconfig.MACHINEKIT_INI
    if not os.path.isfile(mkini):
        sys.stderr.write("MACHINEKIT_INI " + mkini + " does not exist\n")
        sys.exit(1)

    mki = configparser.ConfigParser()
    mki.read(mkini)
    mk_uuid = mki.get("MACHINEKIT", "MKUUID")
    remote = mki.getint("MACHINEKIT", "REMOTE")

    if remote == 0:
        print(
            "Remote communication is deactivated, videoserver will use the loopback interfaces"
        )
        print("set REMOTE in " + mkini + " to 1 to enable remote communication")

    if debug:
        print("announcing videoserver")

    hostname = '%(fqdn)s'  # replaced by service announcement
    video = VideoServer(
        args.ini, svc_uuid=mk_uuid, host=hostname, loopback=(not remote), debug=debug
    )
    video.setDaemon(True)
    video.start()

    for webcam in args.webcams:
        video.start_video(webcam)

    while True:
        time.sleep(1)


if __name__ == "__main__":
    main()
