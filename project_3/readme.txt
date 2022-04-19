Name: Hrishikesh Dadhich
ASU ID: 1222306930

To build
west build -b mimxrt1050_evk

To flash
west flash

Testing steps I followed
0) Connect device to serial output
1) See the output with period 1000
2) Copper client tests
    2.1) Use sensor/period to change the period to 2000
    2.2) See the output on the serial output (changed to 2000)
    2.3) Get LED status of by led/led_r or led/led_g or led/leg_b
    2.4) Put LED using put for led/led_r or led_g or led_b
    2.5) Get the sensor/hcsr_0 (Sometimes sensor does not give value and hence can get 0 as a result)
    2.6) Get the sensor/hcsr_1
    2.7) Observe sensor/hcsr_0
        You will see the "Observation started"
        You will see updated prev and new value if the diff is more than 0.5 inches
    2.8) Cancel the observer
        You will se the "Observation stopped"

Feel free to use different combination later on to test the code but please follow the above steps to grade if my basic code is working.

I tried to create patch using diff but I patch is forming more than 1GB file. I can show in TA office hours.
I have created patch using git diff --staged and also I am attaching modified files in the zephyr for your reference.
I am using zephyr 2.6.0 only.
I am able to compile my code on machine.

In case my patch does not work please manually copy paste driver/sensor/* files /<zephyr>/drive/sensor

I am also attachig build folder renamed as build_HD in case my code does not compile.
