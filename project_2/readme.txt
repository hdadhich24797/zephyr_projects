Name: Hrishikesh Sunil Dadhich
ASU ID: 1222306930

p2 rgb <x1> <x2> <x2>
p2 ledm <row> <val1> <val2> ...
p2 ledb <0 or 1>

NOTE: I am able to implement above all in the main.c code but driver implementation is pending.
I have tried to implement the driver code but due to lack of time I was not able to complete it.

I was able to execute the MAX7218 Display functionality in the main code by using hardcoded values.

To build the code:

west build -b <board_name>

To flash:
west flash

I have written test_rgb_pwm for your refernece at the last line of the code.
