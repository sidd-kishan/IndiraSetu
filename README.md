# IndraSetu

GPIO2 -> CLKOUT
GPIO3 -> RXF#
GPIO4 -> TXE#

GPIO5 -> OE#
GPIO6 -> RD#
GPIO7 -> WR#

GPIO8 -> BIT0

VBUS  -> 3.3v

Sending data which is emiting from pc through ft232h into pi pico with the help of PIO state machine

(PC + FT232H) -> Pi Pico

https://ftdichip.com/wp-content/uploads/2023/09/DS_FT232H.pdf page9
![image](https://github.com/user-attachments/assets/8218383e-ef5a-4f15-b0bc-0f2af2ca4632)

https://ftdichip.com/wp-content/uploads/2020/08/TN_167_FIFO_Basics.pdf page15
![image](https://github.com/user-attachments/assets/0eba0862-4f97-4fa5-8e6f-982e1e37b466)

Refrences:-
[AN130](https://ftdichip.com/wp-content/uploads/2020/08/AN_130_FT2232H_Used_In_FT245-Synchronous-FIFO-Mode.pdf)
[AN135](https://www.ftdichip.com/Documents/AppNotes/AN_135_MPSSE_Basics.pdf)
[AN165](https://www.ftdichip.com/Documents/AppNotes/AN_165_Establishing_Synchronous_245_FIFO_Communications_using_a_Morph-IC-II.pdf)
[AN167](https://ftdichip.com/wp-content/uploads/2020/08/TN_167_FIFO_Basics.pdf)
