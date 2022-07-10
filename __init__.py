from gc import collect
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
    blksize = 4096
    doswap = 1

    def _log(self, msg):
        # scroll check
        if self.window.get_next_line() > self.window.height_chars():
            self.window.cls()
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
        buf = bytearray(self.blksize)
        self._log("got buffer..")
        if self.doswap>1:
            self._log(f"Backing up OTA {ota}={part}..")
            remove('ota.bin')
            with open('ota.bin', 'wb') as f:
                l = 0
                t = part.info()[3]
                while l<t:
                    part.readblocks(l>>12, buf)
                    f.write(buf)
                    l += self.blksize
                    self._log(f"{l}/{t}")
            self._log("written 'ota.bin'..")
        if self.doswap>0:
            self._log(f"Writing 'doom.bin' to OTA {ota}..")
            with open('doom.bin', 'rb') as f:
                t = stat('doom.bin')[6]
                l = 0
                while l<t:
                    n = f.readinto(buf)
                    # round up to block size multiple
                    while n%self.blksize!=0:
                        n += 1
                    part.writeblocks(l>>12, buf)
                    l += self.blksize
                    self._log(f"{l}/{t}")
            self._log("written OTA")
        # allow the GC to clean up..
        buf = None
        collect()
        self._log("loading DOOM!")
        ok = doomloader.doom(ota, self)
        #sleep_ms(5000)
        # swap the OTA partition contents back again
        buf = bytearray(self.blksize)
        self._log("got buffer..")
        if self.doswap>1:
            self._log("Swapping OTA content back again..")
            with open('ota.bin', 'rb') as f:
                t = part.info()[3]
                l = 0
                while l<t:
                    n = f.readinto(buf)
                    while n%self.blksize!=0:
                        n += 1
                    part.writeblocks(l>>12, buf)
                    l += self.blksize
                    self._log(f"{l}/{t}")
            self._log("written OTA")
        self._log("I ain't dead yet!")

    def __call__(self, op, arg1=None, arg2=None, arg3=None, arg4=None):
        self.upcalls += 1
        if "blit" == op:        # first check for speed!
            return display.blit_buffer(arg1, 0, 0, 240, 135)
        elif "sleep" == op:     # timing things next, for accuracy
            return sleep_ms(arg1)
        elif "ticks" == op:
            return ticks_ms()
        try:
            print(f"Doom.callback[{self.upcalls}]({op},{arg1},{arg2},{arg3},{arg4})")
            #sleep_ms(1000)
            if "fopen" == op:
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
                return remove(arg1)
            elif "rename" == op:
                return rename(arg1, arg2)
            elif "mkdir" == op:
                return mkdir(arg1)
            raise OSError(f"unknown command: {op}")
        except BaseException as oops:
            print(oops)
            pass
        return None

main = Doom
