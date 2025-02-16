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

@rp2.asm_pio(sideset_init=(rp2.PIO.OUT_HIGH,rp2.PIO.OUT_HIGH,rp2.PIO.OUT_HIGH),autopush=True, push_thresh=32,fifo_join=rp2.PIO.JOIN_RX,in_shiftdir=rp2.PIO.SHIFT_LEFT)
def wait_pin_low():
	#wrap_target()
	wait(0, gpio, 3).side(0)
	irq(block, rel(0)).side(0)
	wait(0, gpio, 2).side(0)
	wait(1, gpio, 2).side(0)
	#in_(pins,1).side(0b011)
	wrap_target()
	in_(pins,1).side(0)
	wrap()
	#irq(block, rel(0))
	#wrap()


def handler(sm):
	# Print a (wrapping) timestamp, and the state machine object.
	print(time.ticks_ms(), sm)
	pin5.value(0)
	pin6.value(0)
	pin7.value(0)



# Instantiate StateMachine(1) with wait_pin_low program on Pin(17).
sm1 = rp2.StateMachine(0, wait_pin_low,freq= 60_000_000, sideset_base=pin5, in_base=pin8)
sm1.irq(handler)

# Start the StateMachine's running.
#sm0.active(1)
#sm1.active(1)



# Now, when Pin(16) or Pin(17) is pulled low a message will be printed to the REPL.

while True:
    #if pin16.value()==1:
    #    print("1")
    #else:
    #    print("0")
    sm1.active(1)
    qsz=0
    while sm1.rx_fifo():# and qsz<8:
        print(bin(sm1.get()))
        #print(int(sm1.get()))
        #sm_got = bin(sm1.get())[2:]
        #sm_length = 32-len(sm_got)
        #if sm_length :
        #    print("0"*sm_length+sm_got)
        #else:
        #    print(sm_got)
        qsz=qsz+1
    print("------------------"+str(qsz))
    time.sleep(0.2)
    sm1.active(0)
    sm1.restart()
