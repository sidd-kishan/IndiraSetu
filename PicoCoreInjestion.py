import rp2
import machine
import time

pin2 = machine.Pin(2, machine.Pin.IN)  # CLKOUT

pin3 = machine.Pin(3, machine.Pin.IN)  # RF#
pin4 = machine.Pin(4, machine.Pin.IN)  # TXE

pin5 = machine.Pin(5, machine.Pin.OUT) # OE# to be side set
pin6 = machine.Pin(6, machine.Pin.OUT) # RD# to be side set
pin7 = machine.Pin(7, machine.Pin.OUT) # WR# to be side set

pin5.value(1)
pin6.value(1)
pin7.value(1)

pin8 = machine.Pin(8, machine.Pin.IN)  # Bit 0
pin9 = machine.Pin(9, machine.Pin.IN)  # Bit 1

# Define the PIO program
@rp2.asm_pio(sideset_init=(rp2.PIO.OUT_HIGH,rp2.PIO.OUT_HIGH,rp2.PIO.OUT_HIGH), autopush=True, push_thresh=32,fifo_join=rp2.PIO.JOIN_RX,in_shiftdir=rp2.PIO.SHIFT_LEFT)
def pio_program():
    #wrap_target()
    #nop().side(7)
    #wait(0,pin,3).side(7)
    wait(0,gpio,3).side(7)
    irq(block, rel(0)).side(7)
    wait(0,gpio,2).side(7)
    wait(1,gpio,2).side(6)
    #wait(0,pin,1).side(3)
    #irq(block, rel(0)).side(3)
    #wrap()
    wrap_target()
    in_(pins, 1).side(4)  # Read 1 bit from the input pin (GPIO 8)
    wrap()

def handler(sm):
    # Print a (wrapping) timestamp, and the state machine object.
    print(time.ticks_ms(), sm)

# Create a StateMachine with the PIO program, running at 60 MHz
sm = rp2.StateMachine(0, pio_program, freq=60_000_000, sideset_base=pin5, in_base=pin8)
sm.irq(handler)
# Start the StateMachine
#sm.active(1)


# Start the StateMachine

def clear():
    print("\x1B\x5B2J", end="")
    print("\x1B\x5BH", end="")

while True:
    sm.active(1)
    print("-----------------------")
    qsz=0
    while sm.rx_fifo() and qsz < 8:
        print(bin(sm.get()))
        qsz=qsz+1
    time.sleep(0.5)
    sm.restart()
    #clear()
    # Wait for a short period before checking again
    #time.sleep(1)
