from time import sleep_ms, ticks_ms
from tidal import *
from app import TextApp
from . import doomgeneric
from uos import *

class Doom(TextApp):
    BG = BLACK
    FG = WHITE
    upcalls = 0

    def on_activate(self):
        super().on_activate()
        # landscape: joystick on the left, fire on Btn A, weapon on Btn B
        display.rotation(1)
        self.window.println("Hi Mum!")
        chdir("/apps/Doom")
        doomgeneric.doom(self)

    def __call__(self, op, arg1=None, arg2=None, arg3=None, arg4=None):
        self.upcalls += 1
        print(f"Doom.callback[{self.upcalls}]({op},{arg1},{arg2},{arg3},{arg4})")
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
