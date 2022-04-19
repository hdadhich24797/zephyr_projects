Name: Hrishikesh Sunil Dadhich
ASU ID: 1222306930

Buidling application:
    cd trace_app
    west build -b mimxrt1050_evk
    <You can see the "build" folder now>

Flashing application
    cd trace_app
    <build the application>
    west flash

Run minicom or putty or screen. I have used minicom to see logs
    sudo minicom -s
    <Select port (you can check the port using "ls /dev/tty*" and baud rate to 115200>
    <Use "activate" shell command in the serial monitor to activate the threads>
    <The software will show error message if deadline is missed by any thread>


