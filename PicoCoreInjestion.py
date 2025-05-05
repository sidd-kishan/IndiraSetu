# Example using PIO to wait for a pin change and raise an IRQ.
#
# Demonstrates:
# - PIO wrapping
# - PIO wait instruction, waiting on an input pin
# - PIO irq instruction, in blocking mode with relative IRQ number
# - setting the in_base pin for a StateMachine
# - setting an irq handler for a StateMachine
# - instantiating 2x StateMachine's with the same program and different pins

import time
from machine import Pin
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
	wait(1, gpio, 2).side(7)
	wait(0, gpio, 2).side(7)
	set(pindirs,0).side(6)[3]
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
        
main()
