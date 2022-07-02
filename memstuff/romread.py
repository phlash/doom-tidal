# Can I simply read all the IROM space?
from app import TextApp
from . import romread

class RomRead(TextApp):

    def on_activate(self):
        super().on_activate()
        return romread.go()

main = RomRead
