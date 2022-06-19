from time import sleep
from tidal import *
from app import TextApp
from . import doomgeneric
from uos import *

class Doom(TextApp):
    BG = BLACK
    FG = WHITE

    def on_activate(self):
        super().on_activate()
        self.window.println("Hi Mum!")
        chdir("/apps/Doom")
        print(doomgeneric.doom(self))

    def __call__(self, op, arg1=None, arg2=None, arg3=None, arg4=None):
        print(f"Doom.callback({op},{arg1},{arg2},{arg3},{arg4})")
        sleep(2)
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
            try:
                return remove(arg1)
            except OSError:
                pass
        elif "rename" == op:
            try:
                return rename(arg1, arg2)
            except OSError:
                pass
        elif "mkdir" == op:
            try:
                return mkdir(arg1)
            except OSError:
                pass
        print("unknown command!")
        return 0

main = Doom
