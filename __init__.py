from time import sleep_ms, ticks_ms
from tidal import *
from app import TextApp
from . import doomloader
from uos import *
from esp32 import Partition

class Doom(TextApp):
    BG = BLACK
    FG = WHITE
    upcalls = 0
    doswap = 0

    def _log(self, msg):
        self.window.println(msg)
        print(msg)

    def on_activate(self):
        super().on_activate()
        # landscape: joystick on the left, fire on Btn A, weapon on Btn B
        display.rotation(1)
        self._log("Hi Mum!")
        # swap unused OTA partition contents with our app ;=)
        ota = 0 if Partition.RUNNING>1 else 1
        part = Partition.find(Partition.TYPE_APP, label=f"ota_{ota}")[0]
        chdir("/apps/Doom")
        # read the existing contents..
        buf = bytearray(part.info()[3])
        self._log("got buffer..")
        if self.doswap>1:
            self._log(f"Backing up OTA {ota}={part}..")
            part.readblocks(0, buf)
            self._log("read ota..")
            remove('ota.bin')
            with open('ota.bin', 'wb') as f:
                f.write(buf)
            self._log("written 'ota.bin'..")
        if self.doswap>0:
            self._log(f"Writing 'doom.bin' to OTA {ota}..")
            with open('doom.bin', 'rb') as f:
                l = f.readinto(buf)
                # round up to block size multiple
                while l%4096!=0:
                    l += 1
                self._log(f"read 'doom.bin'({l})..")
                part.writeblocks(0, buf[0:l])
            self._log("written OTA")
        # allow the GC to clean up..
        buf = None
        self._log("running DOOM!")
        ok = doomloader.doom(ota, self)
        #sleep_ms(5000)
        # swap the OTA partition contents back again
        buf = bytearray(part.info()[3])
        self._log("got buffer..")
        if self.doswap>1:
            self._log("Swapping OTA content back again..")
            with open('ota.bin', 'rb') as f:
                l = f.readinto(buf)
                self._log(f"read 'ota.bin'({l})..")
                part.writeblocks(0, buf[0:l])
            self._log("written OTA, thanks for playing ;=)")
        self._log("I ain't dead yet!")

    def __call__(self, op, arg1=None, arg2=None, arg3=None, arg4=None):
        self.upcalls += 1
        #print(f"Doom.callback[{self.upcalls}]({op},{arg1},{arg2},{arg3},{arg4})")
        #sleep_ms(1000)
        if "blit" == op:        # first check for speed!
            return display.blit_buffer(arg1, 0, 0, 240, 135)
        elif "sleep" == op:     # timing things next, for accuracy
            return sleep_ms(arg1)
        elif "ticks" == op:
            return ticks_ms()
        elif "fopen" == op:
            return open(arg1, arg2)
        elif "fclose" == op:
            return arg1.close()
        elif "ftell" == op:
            return arg1.tell()
        elif "fseek" == op:
            return arg1.seek(arg2, arg3)
        elif "fwrite" == op:
            return arg2.write(arg1)
        elif "fread" == op:
            return arg2.readinto(arg1)
        elif "remove" == op:
            try:
                return remove(arg1)
            except OSError:
                return 0
        elif "rename" == op:
            try:
                return rename(arg1, arg2)
            except OSError:
                return 0
        elif "mkdir" == op:
            try:
                return mkdir(arg1)
            except OSError:
                return 0
        print("unknown command!")
        return 0

main = Doom
