/*
 *  Assignment2: RTES CSE 522
 *  Done By: Hrishikeh Dadhich
 *  ASU ID: 1222306930
 */

#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <sys/util.h>
#include <sys/printk.h>
#include <shell/shell.h>
#include <shell/shell_uart.h>
#include <version.h>
#include <stdlib.h>
#include <drivers/pwm.h>
#include <devicetree/pwms.h>
#include <fsl_iomuxc.h>
#include <drivers/display.h>
#include <drivers/spi.h>

#define DISP_NODE DT_PROP(DT_NODELABEL(maxdisplay), label)

/*  Flag to blink display driver    */
bool blink_flag = false;

/*  Data input array for lebm command   */
uint8_t input[8];
uint16_t input_16[8];  

/*  Init array to configure the display matrix  */
uint16_t init_array[5] = {0x0f00, 0x0900, 0x0a0f, 0x0b07, 0x0c01};
/*  Clear array to erase the display matrix */
uint16_t clear_array[8] = {0x0100, 0x0200,0x0300,0x0400,0x0500,0x0600,0x0700,0x0800};

struct spi_buf tx;
struct spi_buf_set tx_bufs;

/*  Pointers to device drivers  */
const struct device *pwmb0 = NULL;
const struct device *pwmb1 = NULL;
const struct device *max_display = NULL;

#define DEBUG 

#if defined(DEBUG) 
	#define DPRINTK(fmt, args...) printk("DEBUG: %s():%d: " fmt, \
   		 __func__, __LINE__, ##args)
#else
 	#define DPRINTK(fmt, args...) /* do nothing if not defined*/
#endif

/*  PWM Settings    */
#define LED_PWM_RED DT_NODELABEL(r_led_pwm)

#define LED0_LABEL DT_PROP(LED_PWM_RED, label)
#define LED0_CH  DT_PWMS_CHANNEL(LED_PWM_RED)

#define LED_PWM_GREEN DT_NODELABEL(g_led_pwm)

#define LED1_LABEL DT_PROP(LED_PWM_GREEN, label)
#define LED1_CH  DT_PWMS_CHANNEL(LED_PWM_GREEN)

#define LED_PWM_BLUE DT_NODELABEL(b_led_pwm)

#define LED2_LABEL DT_PROP(LED_PWM_BLUE, label)
#define LED2_CH  DT_PWMS_CHANNEL(LED_PWM_BLUE)

/*  Function to set IOMUX for SPI Pins  */
void set_iomux_spi()
{
    /*  SPI */
    /*  SPI CS */
	IOMUXC_SetPinMux(IOMUXC_GPIO_SD_B0_01_LPSPI1_PCS0, 0);

	IOMUXC_SetPinConfig(IOMUXC_GPIO_SD_B0_01_LPSPI1_PCS0,
			    IOMUXC_SW_PAD_CTL_PAD_PUE(1) |
			    IOMUXC_SW_PAD_CTL_PAD_PKE_MASK |
			    IOMUXC_SW_PAD_CTL_PAD_SPEED(2) |
			    IOMUXC_SW_PAD_CTL_PAD_DSE(6));
    /*  SPI OUT */
	IOMUXC_SetPinMux(IOMUXC_GPIO_SD_B0_02_LPSPI1_SDO, 0);

	IOMUXC_SetPinConfig(IOMUXC_GPIO_SD_B0_02_LPSPI1_SDO,
			    IOMUXC_SW_PAD_CTL_PAD_PUE(1) |
			    IOMUXC_SW_PAD_CTL_PAD_PKE_MASK |
			    IOMUXC_SW_PAD_CTL_PAD_SPEED(2) |
			    IOMUXC_SW_PAD_CTL_PAD_DSE(6));
    /*  SPI CLK */
	IOMUXC_SetPinMux(IOMUXC_GPIO_SD_B0_00_LPSPI1_SCK, 0);

	IOMUXC_SetPinConfig(IOMUXC_GPIO_SD_B0_00_LPSPI1_SCK,
			    IOMUXC_SW_PAD_CTL_PAD_PUE(1) |
			    IOMUXC_SW_PAD_CTL_PAD_PKE_MASK |
			    IOMUXC_SW_PAD_CTL_PAD_SPEED(2) |
			    IOMUXC_SW_PAD_CTL_PAD_DSE(6));

}

/*  Function to set configure max7219 display driver    */
void config_display_driver()
{
    const struct display_driver_api *api = (struct display_driver_api *)max_display->api;
    
    api->write(max_display, 0, 5, NULL, (void *)init_array);
}

/*  Function to clear max7219 display driver    */
void clear_display_matrix()
{
    const struct display_driver_api *api = (struct display_driver_api *)max_display->api;
    
    api->write(max_display, 0, 8, NULL, (void *)clear_array);
}

/*  Function to set IOMUX for PWM Pins  */
void set_iomux_pwm()
{
    /* FOR PWM */
    /*  RED */
	IOMUXC_SetPinMux(IOMUXC_GPIO_AD_B0_11_FLEXPWM1_PWMB03, 0);

	IOMUXC_SetPinConfig(IOMUXC_GPIO_AD_B0_11_FLEXPWM1_PWMB03,
			    IOMUXC_SW_PAD_CTL_PAD_PUE(1) |
			    IOMUXC_SW_PAD_CTL_PAD_PKE_MASK |
			    IOMUXC_SW_PAD_CTL_PAD_SPEED(2) |
			    IOMUXC_SW_PAD_CTL_PAD_DSE(6));

	IOMUXC_SetPinMux(IOMUXC_GPIO_AD_B0_10_FLEXPWM1_PWMA03, 0);

    /*  Green    */
	IOMUXC_SetPinConfig(IOMUXC_GPIO_AD_B0_10_FLEXPWM1_PWMA03,
			    IOMUXC_SW_PAD_CTL_PAD_PUE(1) |
			    IOMUXC_SW_PAD_CTL_PAD_PKE_MASK |
			    IOMUXC_SW_PAD_CTL_PAD_SPEED(2) |
			    IOMUXC_SW_PAD_CTL_PAD_DSE(6));

    /*  Blue    */
	IOMUXC_SetPinMux(IOMUXC_GPIO_SD_B0_03_FLEXPWM1_PWMB01, 0);

	IOMUXC_SetPinConfig(IOMUXC_GPIO_SD_B0_03_FLEXPWM1_PWMB01,
			    IOMUXC_SW_PAD_CTL_PAD_PUE(1) |
			    IOMUXC_SW_PAD_CTL_PAD_PKE_MASK |
			    IOMUXC_SW_PAD_CTL_PAD_SPEED(2) |
			    IOMUXC_SW_PAD_CTL_PAD_DSE(6));


}

/*  Function to test RGB using PWM  */
void test_rgb_pwm()
{
    int count = 0;
    int ret = 0;
    /*  RED */
    while (count <= 100)
    {
        ret = pwm_pin_set_cycles(pwmb0, LED0_CH, 100, count, PWM_POLARITY_NORMAL);
        if (ret != 0)
        {
            DPRINTK("Error in setting cycles, ret = %d\r\n", ret);
        }
        count = count + 20;
        k_msleep(500);
    }
    ret = pwm_pin_set_cycles(pwmb0, LED0_CH, 100, 0, PWM_POLARITY_NORMAL);
    if (ret != 0)
    {
        DPRINTK("Error in setting cycles, ret = %d\r\n", ret);
    }
    count = 0;
    /*  Green */
    while (count <= 100)
    {
        ret = pwm_pin_set_cycles(pwmb0, LED1_CH, 100, count, PWM_POLARITY_NORMAL);
        if (ret != 0)
        {
            DPRINTK("Error in setting cycles, ret = %d\r\n", ret);
        }
        count = count + 20;
        k_msleep(500);
    }
    ret = pwm_pin_set_cycles(pwmb0, LED1_CH, 100, 0, PWM_POLARITY_NORMAL);
    if (ret != 0)
    {
        DPRINTK("Error in setting cycles, ret = %d\r\n", ret);
    }
    count = 0;
    /*  Blue */
    while (count <= 100)
    {
        ret = pwm_pin_set_cycles(pwmb1, LED2_CH, 100, count, PWM_POLARITY_NORMAL);
        if (ret != 0)
        {
            DPRINTK("Error in setting cycles, ret = %d\r\n", ret);
        }
        count = count + 20;
        k_msleep(500);
    }
    ret = pwm_pin_set_cycles(pwmb1, LED2_CH, 100, 0, PWM_POLARITY_NORMAL);
    if (ret != 0)
    {
        DPRINTK("Error in setting cycles, ret = %d\r\n", ret);
    }
}

/*  Callback functioo for RGB   */
static int rgb(const struct shell *shell, size_t argc, char **argv)
{
	int input[3];
	int j = 1;
	for (int i = 0; i < argc - 1; i++)
	{
		input[i] = atoi(argv[j]);
		j++;
	}
    DPRINTK("%d %d %d\r\n", input[0], input[1], input[2]);
    pwm_pin_set_cycles(pwmb0, LED0_CH, 100, input[0], PWM_POLARITY_NORMAL);
    pwm_pin_set_cycles(pwmb0, LED1_CH, 100, input[1], PWM_POLARITY_NORMAL);
    pwm_pin_set_cycles(pwmb1, LED2_CH, 100, input[2], PWM_POLARITY_NORMAL);
    return 0;
}

/*  Callback functioo for LEDM   */
static int ledm(const struct shell *shell, size_t argc, char **argv)
{
    const struct display_driver_api *api = (struct display_driver_api *)max_display->api;
	int j = 2;
	int row = strtol(argv[1], NULL, 16);
    /*  Increment row as user input is 0 index  */
    row++;
    /*  Parse the command line and convert string to integer    */
	for (int i = 0; i < argc - 2; i++)
	{
		input[i] = (int)strtol(argv[j], NULL, 16);
        j++;
	}
    /*  Merge row and data: 0x<row><val>    */
    for (int i = 0; i < argc - 2; i++)
    {
        input_16[i] = 0;
        input_16[i] = input_16[i] | (row + i);
        input_16[i] = input_16[i] << 8;
        input_16[i] = input_16[i] | input[i];
    }
    api->write(max_display, row, argc - 2, NULL, (void *)input_16);
    return 0;
}

/*  Callback functioo for LEDB   */
static int ledb(const struct shell *shell, size_t argc, char **argv)
{
    const struct display_driver_api *api = (struct display_driver_api *)max_display->api;
	int input = atoi(argv[1]);
	if (input == 1)
	{
        /*  Set global flag to start the blinking   */
        blink_flag = true;
	}
	else
	{
        /*  Un-set global flag to stop the blinking   */
        blink_flag = false;
        api->blanking_off(max_display);
	}
    return 0;
}

/*  Creating subcommands (level 1 command) array for command p2    */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_cmd,
	SHELL_CMD(rgb, NULL, "PWM Led Intensity command.", rgb),
	SHELL_CMD(ledm, NULL, "Turning on LED matrix command.", ledm),
	SHELL_CMD(ledb, NULL, "Blinking the pattern on Matrix", ledb),
	SHELL_SUBCMD_SET_END);

/*  Creating root (level 0) command "p2" without a handler  */
SHELL_CMD_REGISTER(p2, &sub_cmd, "List of commands", NULL);

void main(void)
{

    set_iomux_spi();
    /*  DISPLAY binding */
    DPRINTK("Module name is %s\r\n", DISP_NODE);
	max_display=device_get_binding(DISP_NODE);
	if (!max_display) {
		DPRINTK("error 1\n");
		return;
	}
    config_display_driver();
    clear_display_matrix();
    
    set_iomux_pwm();
    
    /*  Red and Green PWM    */
    pwmb0=device_get_binding(LED0_LABEL);
	if (!pwmb0) {
		DPRINTK("error 2\n");
		return;
	}
    /*  Blue PWM    */
	pwmb1=device_get_binding(LED2_LABEL);
	if (!pwmb1) {
		DPRINTK("error 3\n");
		return;
	}
    /*  Unit test for PWM LED   */
    //test_rgb_pwm();
    
    /*  Seperate blinking loop so that uart loop keep on working for more input */
    const struct display_driver_api *api = (struct display_driver_api *)max_display->api;
    while (1)
    {
        bool flag = true;
        while (blink_flag)
        {
            if (flag)
                api->blanking_on(max_display);
            else
                api->blanking_off(max_display);
            k_msleep(1000);
            flag = !flag;
        }
        k_msleep(100);
    }
    return;
}
