from tidal import *
from app import TextApp
from . import doomgeneric

class Doom(TextApp):
    BG = BLACK
    FG = WHITE

    def on_activate(self):
        super().on_activate()
        self.window.println("Hi Mum!")
        print(doomgeneric.doom(self))

main = Doom
