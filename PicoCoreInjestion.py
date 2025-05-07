from machine import Pin, SPI  # type: ignore
from random import random, seed
from ili9341 import Display, color565
from utime import sleep_us, ticks_cpu, ticks_us, ticks_diff  # type: ignore
import _thread
import time
import rp2

pin2=Pin(2,Pin.IN)   #clkout -> AC5
pin3=Pin(3,Pin.IN)   #RXF#   -> AC0
pin4=Pin(4,Pin.IN)   #TXE#   -> ?

pin5=Pin(5,Pin.OUT,Pin.PULL_UP) # OE# -> AC6
pin6=Pin(6,Pin.OUT,Pin.PULL_UP) # RD# -> AC2
pin7=Pin(7,Pin.OUT,Pin.PULL_UP) # WR# -> AC3

pin5.value(1)
pin6.value(1)
pin7.value(1)

pin8=Pin(8,Pin.IN,Pin.PULL_UP)  # 1 bit -> AD0

recv_start = 0

hit = 0
miss = 0
pkt_tot_len = 0
good_pkt_tot_len =0



@rp2.asm_pio(set_init=[rp2.PIO.OUT_LOW],sideset_init=(rp2.PIO.OUT_HIGH,rp2.PIO.OUT_HIGH,rp2.PIO.OUT_HIGH),autopush=True, push_thresh=32,in_shiftdir=rp2.PIO.SHIFT_LEFT)
def wait_pin_low():
	#wrap_target()
	wait(0, gpio, 3).side(7)
	irq(block, rel(0)).side(7)
	pull(block).side(7)
	mov(x,osr).side(7)
	mov(isr,null).side(7)
	set(pindirs,0).side(7)
	wait(1, gpio, 2).side(7)
	wait(0, gpio, 2).side(6)
	#in_(isr,32).side(6)
	label("loop")
	in_(pins,1).side(4)
	jmp(x_dec,"loop").side(4)


def handler(sm):
	# Print a (wrapping) timestamp, and the state machine object.
	global recv_start
	#print(time.ticks_ms(), sm)
	#sm_got = bin(sm.get())[2:]
	#print(sm_got)
	sm.put(128)
	recv_start=1



# Instantiate StateMachine(1) with wait_pin_low program on Pin(17).
sm1 = rp2.StateMachine(0, wait_pin_low,freq=120_000_000, sideset_base=pin5, in_base=pin8)
sm1.irq(handler)

def crc8(data):
    crc = 0x00
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = (crc << 1) ^ 0x07
            else:
                crc <<= 1
            crc &= 0xFF  # Keep it 8-bit
    return crc


# Now, when Pin(16) or Pin(17) is pulled low a message will be printed to the REPL.
def main():
    global recv_start
    global hit
    global miss
    global good_pkt_tot_len
    global pkt_tot_len
    recv_start = 1
    qsz = 0
    while True:
        bin_str = ""
        while pin3.value() == 0:
            sm1.active(1)
            #time.sleep(0.0000001)
            while sm1.rx_fifo():#qsz < 8:
                sm_got = bin(sm1.get())[2:]
                if len(sm_got) > 2:
                    bin_str = bin_str + sm_got
                    #pkt_mark_s = bin_str.find("01011010")
                    if len(bin_str) >127:
                        #print(bin_str[:128])
                        pkt_mark = bin_str.find("10100101")
                        if pkt_mark > -1 and pkt_mark < 10:
                            bin_str = bin_str[pkt_mark+8:]
                            str_got = ""
                            crc_input = bytearray()
                            for i in range(0, len(bin_str[:128]), 8):
                                byte = bin_str[i:i+8]
                                str_got += chr(int(byte, 2))
                                crc_input.append(int(byte, 2))
                            if hex(crc8(crc_input[:12])) == hex(crc_input[12]):
                                hit+=1
                                print("hit: "+str(hit)+" bytes on 8 channels recv: "+str(hit * 12)+" miss: "+str(miss)+" and crc8 verified:" + str_got[:12])
                            else:
                                miss +=1
                    #if len(bin_str) >127 and pkt_mark_s > -1 and pkt_mark_s < 10:
                    #    bin_str = bin_str[pkt_mark_s+8:]
                    #    str_got = ""
                    #    for i in range(0, len(bin_str[:128]), 8):
                    #        byte = bin_str[i:i+8]
                    #        str_got += chr(int(byte, 2))
                    #    print("s: "+str_got)
            sm1.active(0)
        

class Box(object):
    """Bouncing box."""

    def __init__(self, screen_width, screen_height, size, display, color):
        """Initialize box.

        Args:
            screen_width (int): Width of screen.
            screen_height (int): Width of height.
            size (int): Square side length.
            display (ILI9341): display object.
            color (int): RGB565 color value.
        """
        self.size = size
        self.w = screen_width
        self.h = screen_height
        self.display = display
        self.color = color
        # Generate non-zero random speeds between -5.0 and 5.0
        seed(ticks_cpu())
        r = random() * 10.0
        self.x_speed = 5.0 - r if r < 5.0 else r - 10.0
        r = random() * 10.0
        self.y_speed = 5.0 - r if r < 5.0 else r - 10.0

        self.x = self.w / 2.0
        self.y = self.h / 2.0
        self.prev_x = self.x
        self.prev_y = self.y

    def update_pos(self):
        """Update box position and speed."""
        x = self.x
        y = self.y
        size = self.size
        w = self.w
        h = self.h
        x_speed = abs(self.x_speed)
        y_speed = abs(self.y_speed)
        self.prev_x = x
        self.prev_y = y

        if x + size >= w - x_speed:
            self.x_speed = -x_speed
        elif x - size <= x_speed + 1:
            self.x_speed = x_speed

        if y + size >= h - y_speed:
            self.y_speed = -y_speed
        elif y - size <= y_speed + 1:
            self.y_speed = y_speed

        self.x = x + self.x_speed
        self.y = y + self.y_speed

    def draw(self):
        """Draw box."""
        x = int(self.x)
        y = int(self.y)
        size = self.size
        prev_x = int(self.prev_x)
        prev_y = int(self.prev_y)
        self.display.fill_hrect(prev_x - size,
                                prev_y - size,
                                size, size, 0)
        self.display.fill_hrect(x - size,
                                y - size,
                                size, size, self.color)


def test():
    """Bouncing box."""
    try:
        # Baud rate of 40000000 seems about the max
        spi = SPI(1, baudrate=40000000, sck=Pin(10), mosi=Pin(11))
        display = Display(spi, dc=Pin(16), cs=Pin(18), rst=Pin(17))

        display.clear()

        colors = [color565(255, 0, 0),
                  color565(0, 255, 0),
                  color565(0, 0, 255),
                  color565(255, 255, 0),
                  color565(0, 255, 255),
                  color565(255, 0, 255)]
        sizes = [12, 11, 10, 9, 8, 7]
        boxes = [Box(display.width, display.height, sizes[i], display,
                 colors[i]) for i in range(6)]

        while True:
            timer = ticks_us()
            for b in boxes:
                b.update_pos()
                b.draw()
            # Attempt to set framerate to 30 FPS
            timer_dif = 33333 - ticks_diff(ticks_us(), timer)
            if timer_dif > 0:
                sleep_us(timer_dif)

    except KeyboardInterrupt:
        display.cleanup()


second_thread = _thread.start_new_thread(test, ())

main()
