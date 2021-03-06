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
        nl = self.window.get_next_line()
        if nl > self.window.height_chars():
            self.window.cls()
        # newline check
        if msg[len(msg)-1]=='\n':
            self.window.println(msg)
            print(msg)
        else:
            self.window.draw_line(msg, nl * self.window.line_height(), self.FG, self.BG, False)
            print(msg, end='')

    def on_activate(self):
        super().on_activate()
        # landscape: joystick on the left, fire on Btn A, weapon on Btn B
        display.rotation(1)
        self._log("Hi Mum!\n")
        # swap unused OTA partition contents with our app ;=)
        ota = 0 if Partition.RUNNING>1 else 1
        part = Partition.find(Partition.TYPE_APP, label=f"ota_{ota}")[0]
        chdir("/apps/Doom")
        # read the existing contents..
        buf = bytearray(self.blksize)
        self._log("got buffer..\n")
        if self.doswap>1:
            self._log(f"Backing up OTA {ota}={part}..\n")
            remove('ota.bin')
            with open('ota.bin', 'wb') as f:
                l = 0
                t = part.info()[3]
                while l<t:
                    part.readblocks(l>>12, buf)
                    f.write(buf)
                    l += self.blksize
                    self._log(f"{l}/{t}")
            self._log("written 'ota.bin'..\n")
        if self.doswap>0:
            self._log(f"Writing 'doom.bin' to OTA {ota}..\n")
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
            self._log("written OTA\n")
        # allow the GC to clean up..
        buf = None
        collect()
        self._log("loading DOOM!\n")
        ok = doomloader.doom(ota, self)
        #sleep_ms(5000)
        # swap the OTA partition contents back again
        buf = bytearray(self.blksize)
        self._log("got buffer..\n")
        if self.doswap>1:
            self._log("Swapping OTA content back again..\n")
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
            self._log("written OTA\n")
        self._log("I ain't dead yet!\n")

    def __call__(self, op, arg1=None, arg2=None, arg3=None, arg4=None):
        self.upcalls += 1
        if "blit" == op:        # first check for speed!
            return display.blit_buffer(arg1, 0, 0, 240, 135)
        elif "sleep" == op:     # timing things next, for accuracy
            return sleep_ms(arg1)
        elif "ticks" == op:
            return ticks_ms()
        elif "keys" == op:
            # assemble a bitfield of held buttons (rotated!)
            keys = 0
            if 0==JOY_DOWN.value():
                keys |= 1
            if 0==JOY_UP.value():
                keys |= 2
            if 0==JOY_LEFT.value():
                keys |= 4
            if 0==JOY_RIGHT.value():
                keys |= 8
            if 0==JOY_CENTRE.value():
                keys |= 16
            if 0==BUTTON_A.value():
                keys |= 32
            if 0==BUTTON_B.value():
                keys |= 64
            if 0==BUTTON_FRONT.value():
                keys |= 128
            return keys
        try:
            rv = None
            #print(f"Doom.callback[{self.upcalls}]({op},{arg1},{arg2},{arg3},{arg4})=", end='')
            #sleep_ms(1000)
            if "fopen" == op:
                rv = open(arg1, arg2)
            elif "fclose" == op:
                rv = arg1.close()
            elif "ftell" == op:
                rv = arg1.tell()
            elif "fseek" == op:
                rv = arg1.seek(arg2, arg3)
            elif "fwrite" == op:
                rv = arg2.write(arg1)
            elif "fread" == op:
                rv = arg2.readinto(arg1)
            elif "remove" == op:
                rv = remove(arg1)
            elif "rename" == op:
                rv = rename(arg1, arg2)
            elif "mkdir" == op:
                rv = mkdir(arg1)
            else:
                raise OSError(f"unknown command: {op}")
            #print(rv)
            return rv
        except BaseException as oops:
            print(oops)
            pass
        return None

main = Doom
