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

def handle_rising(pin):
    global recv_start
    if recv_start == 1:
        recv_start = 0

def handle_falling(pin):
    print("Pin 3 went LOW!")
    global recv_start
    recv_start = 1
    qsz = 0
    bin_str = ""
    sm1.restart()
    sm1.active(1)
    while sm1.rx_fifo():#qsz < 8:
        sm_got = bin(sm1.get())[2:]
        bin_str = bin_str + sm_got
        qsz = qsz + 1
    #sm1.active(0)
    bin_str = [bin_str[bin_str.find('0101')+5:],bin_str[bin_str.find('10101010')+9:],bin_str[bin_str.find('01010101')+9:],bin_str[bin_str.find('101010')+7:],bin_str[bin_str.find('010101')+7:]]
    for j in range(0, len(bin_str)):
        print(bin_str[j])
        str_got = ""
        for i in range(0, len(bin_str[j]), 8):
            byte = bin_str[j][i:i+8]
            str_got += chr(int(byte, 2)) # char = chr(int(byte, 2)<<1) # This gets only first byte to be decoded
        print(str_got.find("Hello world"))
        print("***********************")

# Attach the interrupt
pin3.irq(trigger=Pin.IRQ_FALLING, handler=handle_falling)
#pin3.irq(trigger=Pin.IRQ_RISING, handler=handle_rising)


@rp2.asm_pio(sideset_init=(rp2.PIO.OUT_HIGH,rp2.PIO.OUT_HIGH,rp2.PIO.OUT_HIGH),autopush=True, push_thresh=32,fifo_join=rp2.PIO.JOIN_RX,in_shiftdir=rp2.PIO.SHIFT_LEFT)
def wait_pin_low():
	#wrap_target()
	wait(0, gpio, 3).side(7)
	mov(isr,null).side(7)
	irq(block, rel(0)).side(7)
	wait(0, gpio, 2).side(7)
	wait(1, gpio, 2).side(6)
	#in_(pins,1).side(0b011)
	wrap_target()
	in_(pins,1).side(4)
	wrap()
	#irq(block, rel(0))
	#wrap()


def handler(sm):
	# Print a (wrapping) timestamp, and the state machine object.
	global recv_start
	#print(time.ticks_ms(), sm)
	#sm_got = bin(sm.get())[2:]
	recv_start=1



# Instantiate StateMachine(1) with wait_pin_low program on Pin(17).
sm1 = rp2.StateMachine(0, wait_pin_low,freq=60_000_000, sideset_base=pin5, in_base=pin8)
sm1.irq(handler)

# Start the StateMachine's running.
#sm0.active(1)
#sm1.active(1)

def bin_to_hex(binary_str):
    # Pad binary string to be multiple of 8 bits
    if len(binary_str) % 8 != 0:
        padding = 8 - (len(binary_str) % 8)
        binary_str += '0' * padding

    # Split into 8-bit chunks
    bytes_list = [binary_str[i:i+8] for i in range(0, len(binary_str), 8)]

    # Convert each to hex using hex() and strip "0x", pad with zero if needed
    hex_str = ''
    print_str = ''
    for b in bytes_list:
        byte_val = int(b, 2)
        print_str += chr(byte_val)  # Append the character corresponding to the byte
        h = hex(byte_val)[2:]  # remove '0x'
        if len(h) == 1:
            h = '0' + h  # pad with zero if necessary
        hex_str += h

    print(print_str)  # Print the corresponding ASCII characters
    return hex_str  # Return the hex string




# Now, when Pin(16) or Pin(17) is pulled low a message will be printed to the REPL.
def main():
    global recv_start
    while True:
        time.sleep(1)
        print("------------------")
        
main()
