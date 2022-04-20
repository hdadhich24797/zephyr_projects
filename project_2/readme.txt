Name: Hrishikesh Sunil Dadhich
ASU ID: 1222306930

p2 rgb <x1> <x2> <x2>
p2 ledm <row> <val1> <val2> ...
p2 ledb <0 or 1>


To build the code:

west build -b <board_name>

To flash:
west flash

I have written test_rgb_pwm for your refernece at the last line of the code.

I am using my driver's API for display driver API calls.
