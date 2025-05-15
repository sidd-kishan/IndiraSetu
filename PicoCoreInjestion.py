from machine import Pin, SPI  # type: ignore
from random import random, seed, randint
from ili9341 import Display, color565
from utime import sleep_us, ticks_cpu, ticks_us, ticks_diff  # type: ignore
import _thread
import time
import rp2
import framebuf
import gc
import os
import math
import heapq

heap = []

heapq.heapify(heap)

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
	sm.put(256)
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
                    if len(bin_str) >255:
                        #print(bin_str[:128])
                        pkt_mark = bin_str.find("10100101")
                        if pkt_mark > -1 and pkt_mark < 10:
                            bin_str = bin_str[pkt_mark+8:]
                            str_got = ""
                            crc_input = bytearray()
                            for i in range(0, len(bin_str[:256]), 8):
                                byte = bin_str[i:i+8]
                                str_got += chr(int(byte, 2))
                                crc_input.append(int(byte, 2))
                            if crc8(crc_input[:6]) == crc_input[12] and crc8(crc_input[6:12]) == crc_input[13]:
                                hit+=1
                                heapq.heappush(heap, [crc_input[0],crc_input[1],crc_input[2],crc_input[3],crc_input[4],crc_input[5],crc_input[6],crc_input[7],crc_input[8],crc_input[9],crc_input[10],crc_input[11],crc_input[12],crc_input[13]])                            
                            elif crc8(crc_input[:6]) == crc_input[12]:
                                heapq.heappush(heap, [crc_input[0],crc_input[1],crc_input[2],crc_input[3],crc_input[4],crc_input[5],0,0,0,0,0,0,crc_input[12],crc_input[13]])
                            elif crc8(crc_input[6:12]) == crc_input[13]:
                                heapq.heappush(heap, [0,0,0,0,0,0,crc_input[6],crc_input[7],crc_input[8],crc_input[9],crc_input[10],crc_input[11],crc_input[12],crc_input[13]])
                            else:
                                miss +=1
                                #print("salvaged trig2")
                                #msg+= str("salvage trig2: "+str(trig2%255)+" sum sum: "+str(crc_input[13]))
                            #if msg !="":
                            #    print(msg)
                            #print("crc8 invalid:"+ str(crc_input[0])+" "+ str(crc_input[1])+" "+ str(crc_input[2])+" " + str(crc_input[3])+" "+ str(crc_input[4])+" "+ str(crc_input[5])+" " + str(crc_input[6])+" "+ str(crc_input[7])+" "+ str(crc_input[8])+" " + str(crc_input[9])+" "+ str(crc_input[10])+" "+ str(crc_input[11])+" " + str(crc_input[12])+" "+ str(crc_input[13]))
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
            display (framebuf): frame buffer.
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
        self.x_speed = r - 5
        r = random() * 10.0
        self.y_speed = r - 5

        self.x = self.w / 2
        self.y = self.h / 2

        self.prev_x = self.x
        self.prev_y = self.y

    def update_pos(self):
        """Update box position and speed."""

        # update position
        self.x += self.x_speed
        self.y += self.y_speed

        # limit checking
        if self.x < 0:
            self.x = 0
            self.x_speed = -self.x_speed
        elif self.x > (self.w - self.size):
            self.x = self.w - self.size
            self.x_speed = -self.x_speed
        if self.y < 0:
            self.y = 0
            self.y_speed = -self.y_speed
        elif self.y > (self.h - self.size):
            self.y = self.h - self.size
            self.y_speed = -self.y_speed

        # extra processing load
        # for num in range(1, 200):
        #     num2 = math.sqrt(num)

    def draw(self):
        """Draw box."""
        try:
            top = heapq.heappop(heap)
        except:
            x = int(self.x)
            y = int(self.y)
            size = self.size
            #self.display.fill_rect(x, y, size, size, self.color)
            return
        print("verified:"+ str(top[0])+" "+ str(top[1])+" "+ str(top[2])+" " + str(top[3])+" "+ str(top[4])+" "+ str(top[5])+" " + str(top[6])+" "+ str(top[7])+" "+ str(top[8])+" " + str(top[9])+" "+ str(top[10])+" "+ str(top[11])+" " + str(top[12])+" "+ str(top[13]))
        self.display.line(top[0], top[1], top[2], top[3], top[12]*top[13])
        self.display.line(top[2], top[3], top[4], top[5], top[12]*top[13])
        self.display.line(top[4], top[5], top[0], top[1], top[12]*top[13])
        self.display.line(top[6], top[7], top[8], top[9], top[12]*top[13])
        self.display.line(top[8], top[9], top[10], top[11], top[12]*top[13])
        self.display.line(top[10], top[11], top[6], top[7], top[12]*top[13])


"""
Show memory usage
"""
def free(full=False):
    gc.collect()
    F = gc.mem_free()
    A = gc.mem_alloc()
    T = F + A
    P = '{0:.2f}%'.format(F / T * 100)
    if not full:
        return P
    else:
        return ('Total:{0} Free:{1} ({2})'.format(T, F, P))


# set landscape screen
screen_width = 320
screen_height = 240
screen_rotation = 90

# FrameBuffer needs 2 bytes for every RGB565 pixel
buffer_width = 320
buffer_height = 240
buffer = bytearray(buffer_width * buffer_height * 2)
fbuf = framebuf.FrameBuffer(buffer, buffer_width, buffer_height, framebuf.RGB565)


def test():
    """Bouncing box."""
    global fbuf, buffer, buffer_width, buffer_height
    try:
        # Baud rate of 40000000 seems about the max
        spi = SPI(1, baudrate=40000000, sck=Pin(10), mosi=Pin(11))
        display = Display(spi, dc=Pin(16), cs=Pin(18), rst=Pin(17),width=screen_width, height=screen_height, rotation=screen_rotation)

        display.clear()

        boxes = [Box(1,1, randint(7, 40), fbuf,
                     color565(randint(30, 256), randint(30, 256), randint(30, 256))) for i in range(5)]

        print(free())

        start_time = ticks_us()
        frame_count = 0
        while True:

            for b in boxes:
                b.update_pos()

            for b in boxes:
                b.draw()

            # render display
            display.block(int((320 - buffer_width) / 2), int((240 - buffer_height) / 2),
                          int((320 - buffer_width) / 2) + buffer_width - 1,
                          int((240 - buffer_height) / 2) + buffer_height - 1, buffer)
            # clear buffer
            #fbuf.fill(0)

            frame_count += 1
            if frame_count == 100:
                frame_rate = 100 / ((ticks_us() - start_time) / 1000000)
                print(frame_rate)
                start_time = ticks_us()
                frame_count = 0

    except KeyboardInterrupt:
        display.cleanup()


second_thread = _thread.start_new_thread(test, ())

main()
