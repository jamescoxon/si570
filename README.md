Code for interfacing an arduino with the si570 chip

working_cw1 and working_cw2 - code to do slow CW with the si570 at various
frequencies.

rtty1 - 50 baud RTTY with the si570 and an arduino
	
Note - initially when changing the frequency of the si570 rapidly (e.g. 50baud) 
the arduino would often hang, found that if you pulled the OE pin low before 
changing the frequency and then once changed high again this avoided it hanging.

To calculate the required hex to set a frequency I use ZS6BUJ's xls:
www.zs6buj.com/files/General/ZS6BUJ_Si570_Calculator.xls
