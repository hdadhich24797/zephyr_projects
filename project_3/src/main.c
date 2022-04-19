/*
 * RTES Assignemnt 3
 * Done by: Hrishikesh Dadhich
 * ASU ID: 1222306930
 */

#include <kernel.h>
#include <string.h>
#include <zephyr.h>
#include <device.h>
#include <drivers/sensor.h>
#include <stdio.h>
#include <sys/__assert.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);
#include <drivers/gpio.h>
#include <errno.h>
#include <sys/printk.h>
#include <sys/byteorder.h>
#include <zephyr.h>
#include <net/net_if.h>
#include <net/net_core.h>
#include <net/net_context.h>
#include <net/net_mgmt.h>
#include <net/socket.h>
#include <net/net_mgmt.h>
#include <net/net_ip.h>
#include <net/udp.h>
#include <net/coap.h>
#include <net/coap_link_format.h>
#include <math.h>
#include <timing/timing.h>

/*  To incldue net_hexdump info */
#include "net_private.h"

/*  Labels for Distance sensor  */
#define HC0 DT_NODELABEL(us0)
#define HC0_LBL DT_PROP (HC0, label)
#define HC1 DT_NODELABEL(us1)
#define HC1_LBL DT_PROP (HC1, label)

/*  Labels for LEDs */
#define LED_RED DT_NODELABEL(r_led)
#define LED0    DT_GPIO_LABEL(LED_RED, gpios)
#define PIN0    DT_GPIO_PIN(LED_RED, gpios)
#define FLAGS0  DT_GPIO_FLAGS(LED_RED, gpios)
#define LED_GREEN DT_NODELABEL(g_led)
#define LED1    DT_GPIO_LABEL(LED_GREEN, gpios)
#define PIN1    DT_GPIO_PIN(LED_GREEN, gpios)
#define FLAGS1  DT_GPIO_FLAGS(LED_GREEN, gpios)
#define LED_BLUE DT_NODELABEL(b_led)
#define LED2    DT_GPIO_LABEL(LED_BLUE, gpios)
#define PIN2    DT_GPIO_PIN(LED_BLUE, gpios)
#define FLAGS2  DT_GPIO_FLAGS(LED_BLUE, gpios)

/*  Default values from the coap server example */
#define MAX_COAP_MSG_LEN 256
#define MY_COAP_PORT 5683
#define BLOCK_WISE_TRANSFER_SIZE_GET 2048
#define NUM_OBSERVERS 3
#define NUM_PENDINGS 3
#define STACKSIZE 1024

enum observe_state {OBS_ON = 0, OBS_RUNNING, OBS_OFF};  /*  States to know observation state    */
int g_period = 1000;    /*  Initial period  */

/*  Global values for sensor0 and sensor1   */
float g_prev0 = 0.0f;
float g_curr0 = 0.0f;
float g_prev1 = 0.0f;
float g_curr1 = 0.0f;

/*  Observation flags for sensor0 and sensor1   */
bool obs_flag1 = false;
bool obs_flag2 = false;

/*  Obsercation resource pointers for sensor0 and sensor1   */
static struct coap_resource *sensor1_obs_resource;
static struct coap_resource *sensor2_obs_resource;

/*  device pointers for sensor0 and sensor2 */
const struct device *dev, *dev1;
/*  GPIO devices for LED    */
const struct device *gpio1, *gpio3;

/*  Resource paths  */
static const char * const sensor_path0[] = { "sensor", "hcsr_0", NULL };
static const char * const sensor_path1[] = { "sensor", "hcsr_1", NULL };
static const char * const sensor_period[] = { "sensor", "period", NULL };
static const char * const led_r_path[] = { "led", "led_r", NULL };
static const char * const led_g_path[] = { "led", "led_g", NULL };
static const char * const led_b_path[] = { "led", "led_b", NULL };

/* CoAP socket variables form coap sample server example */
static int sock;
static struct coap_observer observers[NUM_OBSERVERS];
static struct coap_pending pendings[NUM_PENDINGS];
static struct k_work_delayable retransmit_work;
struct sensor_value distance;

bool verify_diff(float f1, float f2);
static int calculate_distance(const struct device *dev);
static int send_notification_packet(const struct sockaddr *addr,
				    socklen_t addr_len,
				    uint16_t age, uint16_t id,
				    const uint8_t *token, uint8_t tkl,
				    bool is_response, int sensor_id, int obs_state);

/*  Sensor thread variales  */
K_THREAD_STACK_DEFINE(tstack, STACKSIZE);
k_tid_t sensor_thread_id;
struct k_thread g_sensor_thread;
struct k_sem g_sem;

void period_timer_handler(struct k_timer *timer);
K_TIMER_DEFINE(period_timer, period_timer_handler, NULL);

/*  Give the semaphore after g_period MSECS */
void period_timer_handler(struct k_timer *timer)
{
    ARG_UNUSED(timer);
    k_sem_give(&g_sem);
}

/*  Sensor thread function  */
void sensor_compute(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    double temp = 0;
    while (1)
    {
        LOG_INF("Inside sensor_compute with period %d", g_period);
        /*  For invalid period input    */
        if (g_period == 0)
            g_period = 1000;
        k_timer_start(&period_timer, K_MSEC(g_period), K_NO_WAIT);
        /*  Calculate distance for sensor0  */
        g_curr0 = 0.0f;
        if (calculate_distance(dev) == 0)
        {
            temp = sensor_value_to_double(&distance);
            g_curr0 = (float) temp;
        }
        k_msleep(10);
        /*  Calculate distance for sensor1  */
        g_curr1 = 0.0f;
        if (calculate_distance(dev1) == 0)
        {
            temp = sensor_value_to_double(&distance);
            g_curr1 = (float) temp;
        }
        if (obs_flag1 || obs_flag2)
        {
            /*  Notify if observe flag and distance is more than 0.5 inches */
            if (obs_flag1 && sensor1_obs_resource)
            {
                if (verify_diff(g_curr0, g_prev0))
                {
                    coap_resource_notify(sensor1_obs_resource);
                    g_prev0 = g_curr0;
                }
            }
            if (obs_flag2 && sensor2_obs_resource)
            {
                if (verify_diff(g_curr1, g_prev1))
                {
                    coap_resource_notify(sensor2_obs_resource);
                    g_prev1 = g_curr1;
                }
            }
        }
        /*  Wait for g_period time  */
        k_sem_take(&g_sem, K_FOREVER);
    }
}

/*  Start the sensor thread */
void init_sensor_thread()
{
    k_sem_init(&g_sem, 0, 1);
    sensor_thread_id = k_thread_create(&g_sensor_thread, tstack, STACKSIZE, sensor_compute, NULL, NULL, NULL, 10, 0, K_NO_WAIT);
}

/*  If distancce is more than 0.5 then return true else false   */
bool verify_diff(float f1, float f2)
{
    if ((f1 - f2 >= 0.5) || (f1 - f2 <= -0.5))
        return true;
    return false;
}

/*  function to calculate the distance using distance sensor from github sample    */
static int calculate_distance(const struct device *dev)
{
    int ret;
    if (!dev)
        return -1;
    struct sensor_driver_api *api = (struct sensor_driver_api *)dev->api;
    ret = api->sample_fetch(dev, SENSOR_CHAN_ALL);
    switch (ret) {
    case 0:
        ret = api->channel_get(dev, SENSOR_CHAN_DISTANCE, &distance);
        if (ret) {
            LOG_ERR("sensor_channel_get failed ret %d", ret);
            return ret;
        }
        LOG_INF("%s: %d.%02dI", dev->name, distance.val1, (distance.val2 / 1000));
        return 0; 
    case -EIO:
        LOG_WRN("%s: Could not read device", dev->name);
        return -1;
    default:

        LOG_ERR("Error when reading device: %s", dev->name);
        return -1;
    }
    return 0;
}

/*  Copied from coap server example */
static int start_coap_server(void)
{
	struct sockaddr_in addr;
	int r;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(MY_COAP_PORT);

	sock = socket(addr.sin_family, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0) {
		LOG_ERR("Failed to create UDP socket %d", errno);
		return -errno;
	}

	r = bind(sock, (struct sockaddr *)&addr, sizeof(addr));
	if (r < 0) {
		LOG_ERR("Failed to bind UDP socket %d", errno);
		return -errno;
	}

	return 0;
}

/*  Copied from coap server example */
static int send_coap_reply(struct coap_packet *cpkt,
			   const struct sockaddr *addr,
			   socklen_t addr_len)
{
	int r;

	net_hexdump("Response", cpkt->data, cpkt->offset);

	r = sendto(sock, cpkt->data, cpkt->offset, 0, addr, addr_len);
	if (r < 0) {
		LOG_ERR("Failed to send %d", errno);
		r = -errno;
	}

	return r;
}

/*  Copied from coap server example */
static int well_known_core_get(struct coap_resource *resource,
			       struct coap_packet *request,
			       struct sockaddr *addr, socklen_t addr_len)
{
	struct coap_packet response;
	uint8_t *data;
	int r;

	data = (uint8_t *)k_malloc(MAX_COAP_MSG_LEN);
	if (!data) {
		return -ENOMEM;
	}

	r = coap_well_known_core_get(resource, request, &response,
				     data, MAX_COAP_MSG_LEN);
	if (r < 0) {
		goto end;
	}

	r = send_coap_reply(&response, addr, addr_len);

end:
    if (data)
    {
        k_free(data);
    }

	return r;
}

/*  Get API for LED get API */
static int led_get(struct coap_resource *resource,
			 struct coap_packet *request,
			 struct sockaddr *addr, socklen_t addr_len)
{
    LOG_INF("Inside my function at line number %d\r\n", __LINE__);

    struct coap_packet response;
	uint8_t payload[40];
	uint8_t token[COAP_TOKEN_MAX_LEN];
	uint8_t *data;
	uint16_t id;
	uint8_t code;
	uint8_t type;
	uint8_t tkl;
	int r;

	code = coap_header_get_code(request);
	type = coap_header_get_type(request);
	id = coap_header_get_id(request);
	tkl = coap_header_get_token(request, token);

	LOG_INF("*******");
	LOG_INF("type: %u code %u id %u", type, code, id);
	LOG_INF("*******");

	if (type == COAP_TYPE_CON) {
		type = COAP_TYPE_ACK;
	} else {
		type = COAP_TYPE_NON_CON;
	}

	data = (uint8_t *)k_malloc(MAX_COAP_MSG_LEN);
	if (!data) {
		return -ENOMEM;
	}

	r = coap_packet_init(&response, data, MAX_COAP_MSG_LEN,
			     COAP_VERSION_1, type, tkl, token,
			     COAP_RESPONSE_CODE_CONTENT, id);
	if (r < 0) {
		goto end;
	}

	r = coap_append_option_int(&response, COAP_OPTION_CONTENT_FORMAT,
				   COAP_CONTENT_FORMAT_TEXT_PLAIN);
	if (r < 0) {
		goto end;
	}

	r = coap_packet_append_payload_marker(&response);
	if (r < 0) {
		goto end;
	}
    
    char s[10];
    int val = 0;

    /*  Compare RGB resource path and turn on or off LED according to the payload   */
    if (strcmp((const char *)led_r_path, (const char *)resource->path) == 0)
    {
        LOG_INF("HERE0");
        strcpy(s, "LED_R");
        val = gpio_pin_get(gpio1, PIN0);
    }
    else if (strcmp((const char *)led_g_path, (const char *)resource->path) == 0)
    {
        LOG_INF("HERE1");
        strcpy(s, "LED_G");
        val = gpio_pin_get(gpio1, PIN1);
    }
    else if (strcmp((const char *)led_b_path, (const char *)resource->path) == 0)
    {
        LOG_INF("HERE2");
        strcpy(s, "LED_B");
        val = gpio_pin_get(gpio3, PIN2);
    }
	r = snprintk((char *) payload, sizeof(payload),
		     "%s, val is %d",s, val);
	if (r < 0) {
		goto end;
	}

	r = coap_packet_append_payload(&response, (uint8_t *)payload,
				       strlen(payload));
	if (r < 0) {
		goto end;
	}

	r = send_coap_reply(&response, addr, addr_len);

end:
    if (data)
    {
        k_free(data);
    }

	return r;
}

/*  Get and observe get for sensor resource */
static int sensor_get(struct coap_resource *resource,
			 struct coap_packet *request,
			 struct sockaddr *addr, socklen_t addr_len)
{
    LOG_INF("Inside my function at line number %d\r\n", __LINE__);

	struct coap_observer *observer;
    struct coap_packet response;
	uint8_t payload[40];
	uint8_t token[COAP_TOKEN_MAX_LEN];
	uint8_t *data;
	uint16_t id;
	uint8_t code;
	uint8_t type;
	uint8_t tkl;
	int r = 0;
    bool sensor_flag1 = false;
    bool sensor_flag2 = false;
    char s[10];
	int sensor_id = 0;
    code = coap_header_get_code(request);
	type = coap_header_get_type(request);
	id = coap_header_get_id(request);
	tkl = coap_header_get_token(request, token);

	LOG_INF("*******");
	LOG_INF("type: %u code %u id %u", type, code, id);
	LOG_INF("*******");
    
    /*  Compare paths to get the sensor information */
    if (strcmp((const char *)sensor_path0, (const char *)resource->path) == 0)
    {
        sensor_flag1 = true;
        sensor_id = 0;
        strcpy(s, "SENSOR0");
    }
    else if (strcmp((const char *)sensor_path1, (const char *)resource->path) == 0)
    {
        sensor_flag2 = true;
        sensor_id = 1;
        strcpy(s, "SENSOR1");
    }
	if (type == COAP_TYPE_CON) {
		type = COAP_TYPE_ACK;
	} else {
		type = COAP_TYPE_NON_CON;
	}
	

	data = (uint8_t *)k_malloc(MAX_COAP_MSG_LEN);
	if (!data) {
		return -ENOMEM;
	}

    /*  Compare COAP option observe field to know if request is GET or OBS On or OBS Off    */
    int obs_field = coap_get_option_int(request, COAP_OPTION_OBSERVE); 
    LOG_INF("obs field is %d", obs_field);
    if (obs_field == 0)
    {
        observer = coap_observer_next_unused(observers, NUM_OBSERVERS);
        if (!observer) {
            LOG_INF("FULL observers");
            return -ENOMEM;
        }
        coap_observer_init(observer, request, addr);

        coap_register_observer(resource, observer);
        LOG_INF("Obs ON");
        if (sensor_flag1)
        {
            sensor1_obs_resource = resource;
            obs_flag1 = true;
        }
        else if (sensor_flag2)
        {
            sensor2_obs_resource = resource;
            obs_flag2 = true;
        }
        send_notification_packet(addr, addr_len,
                resource->age,
                id, token, tkl, true, sensor_id, OBS_ON);
    }
    else if (obs_field == 1)
    {
        LOG_INF("Obs off");
        if (sensor_flag1)
        {
            obs_flag1 = false;
        }
        else if (sensor_flag2)
        {
            obs_flag2 = false;
        }
        send_notification_packet(addr, addr_len,
                0,
                id, token, tkl, true, sensor_id, OBS_OFF);
    }
    else
    {
        LOG_INF("Normal get requet");
        r = coap_packet_init(&response, data, MAX_COAP_MSG_LEN,
                COAP_VERSION_1, type, tkl, token,
                COAP_RESPONSE_CODE_CONTENT, id);
        if (r < 0) {
            goto end;
        }

        r = coap_append_option_int(&response, COAP_OPTION_CONTENT_FORMAT,
                COAP_CONTENT_FORMAT_TEXT_PLAIN);
        if (r < 0) {
            goto end;
        }

        r = coap_packet_append_payload_marker(&response);
        if (r < 0) {
            goto end;
        }
        /*  Send to respected sensor    */
        if (sensor_flag1)
        {
            (void)snprintk((char *) payload, sizeof(payload),
                    "%s : %0.2fI", s, g_curr0);
            g_prev0 = g_curr0;
        }
        else if (sensor_flag2)
        {
            (void)snprintk((char *) payload, sizeof(payload),
                    "%s : %0.2fI", s, g_curr1);
            g_prev1 = g_curr1;
        }
        r = coap_packet_append_payload(&response, (uint8_t *)payload,
                strlen(payload));
        if (r < 0) {
            goto end;
        }
        r = send_coap_reply(&response, addr, addr_len);
    }

end:
    if (data)
    {
        k_free(data);
    }

	return r;
}


/*  Put for led resource */
static int led_put(struct coap_resource *resource,
		    struct coap_packet *request,
		    struct sockaddr *addr, socklen_t addr_len)
{
	struct coap_packet response;
	uint8_t token[COAP_TOKEN_MAX_LEN];
	const uint8_t *payload;
	uint8_t *data;
	uint16_t payload_len;
	uint8_t code;
	uint8_t type;
	uint8_t tkl;
	uint16_t id;
	int r;

    int led_val = 0;

	code = coap_header_get_code(request);
	type = coap_header_get_type(request);
	id = coap_header_get_id(request);
	tkl = coap_header_get_token(request, token);

	LOG_INF("*******");
	LOG_INF("type: %u code %u id %u", type, code, id);
	LOG_INF("*******");
    
	payload = coap_packet_get_payload(request, &payload_len);
	if (payload) {
		net_hexdump("PUT Payload", payload, payload_len);
        /*  Convert ASCII val to int    */
        led_val = (int) *payload;
        led_val = led_val - 48;
        LOG_INF("led val is %d\r\n", led_val);
	}

    if (strcmp((const char *)led_r_path, (const char *)resource->path) == 0)
    {
        gpio_pin_set(gpio1, PIN0, led_val);
    }
    else if (strcmp((const char *)led_g_path, (const char *)resource->path) == 0)
    {
        gpio_pin_set(gpio1, PIN1, led_val);
    }
    else if (strcmp((const char *)led_b_path, (const char *)resource->path) == 0)
    {
        gpio_pin_set(gpio3, PIN2, led_val);
    }

	if (type == COAP_TYPE_CON) {
		type = COAP_TYPE_ACK;
	} else {
		type = COAP_TYPE_NON_CON;
	}

	data = (uint8_t *)k_malloc(MAX_COAP_MSG_LEN);
	if (!data) {
		return -ENOMEM;
	}

	r = coap_packet_init(&response, data, MAX_COAP_MSG_LEN,
			     COAP_VERSION_1, type, tkl, token,
			     COAP_RESPONSE_CODE_CHANGED, id);
	if (r < 0) {
		goto end;
	}

	r = send_coap_reply(&response, addr, addr_len);

end:
    if (data)
    {
        k_free(data);
    }

	return r;
}

/*  Put for sensor resource */
static int sensor_period_put(struct coap_resource *resource,
		    struct coap_packet *request,
		    struct sockaddr *addr, socklen_t addr_len)
{
	struct coap_packet response;
	uint8_t token[COAP_TOKEN_MAX_LEN];
	const uint8_t *payload;
	uint8_t *data;
	uint16_t payload_len;
	uint8_t code;
	uint8_t type;
	uint8_t tkl;
	uint16_t id;
	int r;
    int l_period = 0;


	code = coap_header_get_code(request);
	type = coap_header_get_type(request);
	id = coap_header_get_id(request);
	tkl = coap_header_get_token(request, token);

	LOG_INF("*******");
	LOG_INF("type: %u code %u id %u", type, code, id);
	LOG_INF("*******");

	payload = coap_packet_get_payload(request, &payload_len);
	if (payload) {
		net_hexdump("PUT Payload", payload, payload_len);
        /*  Change period function  */
        for (int i = 0; i < payload_len; i++)
        {
            l_period = l_period + (payload[i] - 48) * pow(10, payload_len - i -1);
        }
        g_period = l_period;	
        LOG_INF("Period is changed to %dms\r\n", g_period);
    }

	if (type == COAP_TYPE_CON) {
		type = COAP_TYPE_ACK;
	} else {
		type = COAP_TYPE_NON_CON;
	}

	data = (uint8_t *)k_malloc(MAX_COAP_MSG_LEN);
	if (!data) {
		return -ENOMEM;
	}

	r = coap_packet_init(&response, data, MAX_COAP_MSG_LEN,
			     COAP_VERSION_1, type, tkl, token,
			     COAP_RESPONSE_CODE_CHANGED, id);
	if (r < 0) {
		goto end;
	}

	r = send_coap_reply(&response, addr, addr_len);

end:
    if (data)
    {
        k_free(data);
    }

	return r;
}

/*  Coap server example */
static void retransmit_request(struct k_work *work)
{
	struct coap_pending *pending;

	pending = coap_pending_next_to_expire(pendings, NUM_PENDINGS);
	if (!pending) {
		return;
	}

	if (!coap_pending_cycle(pending)) {
		k_free(pending->data);
		coap_pending_clear(pending);
		return;
	}

	k_work_reschedule(&retransmit_work, K_MSEC(pending->timeout));
}

/*  Coap server example */
static int create_pending_request(struct coap_packet *response,
				  const struct sockaddr *addr)
{
	struct coap_pending *pending;
	int r;

	pending = coap_pending_next_unused(pendings, NUM_PENDINGS);
	if (!pending) {
		return -ENOMEM;
	}

	r = coap_pending_init(pending, response, addr,
			      COAP_DEFAULT_MAX_RETRANSMIT);
	if (r < 0) {
		return -EINVAL;
	}

	coap_pending_cycle(pending);

	pending = coap_pending_next_to_expire(pendings, NUM_PENDINGS);
	if (!pending) {
		return 0;
	}

	k_work_reschedule(&retransmit_work, K_MSEC(pending->timeout));

	return 0;
}

/*  Function to send notification to observer */
static int send_notification_packet(const struct sockaddr *addr,
				    socklen_t addr_len,
				    uint16_t age, uint16_t id,
				    const uint8_t *token, uint8_t tkl,
				    bool is_response, int sensor_id, int obs_state)
{
	struct coap_packet response;
	char payload[30];
	uint8_t *data;
	uint8_t type;
	int r;

	if (is_response) {
		type = COAP_TYPE_ACK;
	} else {
		type = COAP_TYPE_CON;
	}

	if (!is_response) {
		id = coap_next_id();
	}

	data = (uint8_t *)k_malloc(MAX_COAP_MSG_LEN);
	if (!data) {
		return -ENOMEM;
	}

	r = coap_packet_init(&response, data, MAX_COAP_MSG_LEN,
			     COAP_VERSION_1, type, tkl, token,
			     COAP_RESPONSE_CODE_CONTENT, id);
	if (r < 0) {
		goto end;
	}

	if (age >= 2U) {
		r = coap_append_option_int(&response, COAP_OPTION_OBSERVE, age);
		if (r < 0) {
			goto end;
		}
	}

	r = coap_append_option_int(&response, COAP_OPTION_CONTENT_FORMAT,
				   COAP_CONTENT_FORMAT_TEXT_PLAIN);
	if (r < 0) {
		goto end;
	}

	r = coap_packet_append_payload_marker(&response);
	if (r < 0) {
		goto end;
	}

    float prev = 0.0f;
    float curr = 0.0f;
	/* The response that coap-client expects */
    if (sensor_id == 0)
    {
        prev = g_prev0;
        curr = g_curr0;
    }
    else
    {
        prev = g_prev1;
        curr = g_curr1;
    }
    if (obs_state == OBS_RUNNING)
    {
        /*  Observation is going on */
        r = snprintk((char *) payload, sizeof(payload),
                "Old value %0.2f New value %0.2f\n", prev, curr);
    }
    else if (obs_state == OBS_ON)
    {
        /*  Observation On request ACK  */
        r = snprintk((char *) payload, sizeof(payload),
                "Observation has started\n");
    }
    else if (obs_state == OBS_OFF)
    {
        /*  Observation OFF request ACK  */
        r = snprintk((char *) payload, sizeof(payload),
                "Observation has ended\n");
    }
    if (r < 0) {
        goto end;
    }

	r = coap_packet_append_payload(&response, (uint8_t *)payload,
				       strlen(payload));
	if (r < 0) {
		goto end;
	}

	if (type == COAP_TYPE_CON) {
		r = create_pending_request(&response, addr);
		if (r < 0) {
			goto end;
		}
	}

	r = send_coap_reply(&response, addr, addr_len);

end:
	return r;
}

/*  Notify API for sensor resource  */
static void sensor_notify(struct coap_resource *resource,
		       struct coap_observer *observer)
{
    LOG_INF("sensor notify");
    int sensor_id = 0;
    if (strcmp((const char *)sensor_path0, (const char *)resource->path) == 0)
    {
        sensor_id = 0;
    }
    else if (strcmp((const char *)sensor_path1, (const char *)resource->path) == 0)
    {
        sensor_id = 1;
    }
	send_notification_packet(&observer->addr,
				 sizeof(observer->addr),
				 resource->age, 0,
				 observer->token, observer->tkl, false, sensor_id, OBS_RUNNING);
}

/*  Resource structures declaration for COAP    */
static struct coap_resource resources[] = {
	{ .get = well_known_core_get,
	  .path = COAP_WELL_KNOWN_CORE_PATH,
	},
	{ .get = sensor_get,
	  .notify = sensor_notify,
	  .path = sensor_path0
	},
	{ .get = sensor_get,
	  .notify = sensor_notify,
	  .path = sensor_path1
	},
	{ .put = sensor_period_put,
	  .path = sensor_period
	},
	{ .get = led_get,
      .put = led_put,
	  .path = led_r_path
	},
	{ .get = led_get,
      .put = led_put,
	  .path = led_g_path
	},
	{ .get = led_get,
      .put = led_put,
	  .path = led_b_path
	},
	{ },
};

/*  Coap server example */
static struct coap_resource *find_resouce_by_observer(
		struct coap_resource *resources, struct coap_observer *o)
{
	struct coap_resource *r;

	for (r = resources; r && r->path; r++) {
		sys_snode_t *node;

		SYS_SLIST_FOR_EACH_NODE(&r->observers, node) {
			if (&o->list == node) {
				return r;
			}
		}
	}

	return NULL;
}

/*  Coap server example */
static void process_coap_request(uint8_t *data, uint16_t data_len,
				 struct sockaddr *client_addr,
				 socklen_t client_addr_len)
{
	struct coap_packet request;
	struct coap_pending *pending;
	struct coap_option options[16] = { 0 };
	uint8_t opt_num = 16U;
	uint8_t type;
	int r;

	r = coap_packet_parse(&request, data, data_len, options, opt_num);
	if (r < 0) {
		LOG_ERR("Invalid data received (%d)\n", r);
		return;
	}

	type = coap_header_get_type(&request);

	pending = coap_pending_received(&request, pendings, NUM_PENDINGS);
	if (!pending) {
		goto not_found;
	}

	/* Clear CoAP pending request */
	if (type == COAP_TYPE_ACK) {
		k_free(pending->data);
		coap_pending_clear(pending);
	}

	return;

not_found:

	if (type == COAP_TYPE_RESET) {
		struct coap_resource *r;
		struct coap_observer *o;

		o = coap_find_observer_by_addr(observers, NUM_OBSERVERS,
					       client_addr);
		if (!o) {
			LOG_ERR("Observer not found\n");
			goto end;
		}

		r = find_resouce_by_observer(resources, o);
		if (!r) {
			LOG_ERR("Observer found but Resource not found\n");
			goto end;
		}

		coap_remove_observer(r, o);

		return;
	}

end:
	r = coap_handle_request(&request, resources, options, opt_num,
				client_addr, client_addr_len);
	if (r < 0) {
		LOG_WRN("No handler for such request (%d)\n", r);
	}
}

/*  Coap server example */
static int process_client_request(void)
{
	int received;
	struct sockaddr client_addr;
	socklen_t client_addr_len;
	uint8_t request[MAX_COAP_MSG_LEN];

	do {
		client_addr_len = sizeof(client_addr);
		received = recvfrom(sock, request, sizeof(request), 0,
				    &client_addr, &client_addr_len);
		if (received < 0) {
			LOG_ERR("Connection error %d", errno);
			return -errno;
		}

		process_coap_request(request, received, &client_addr,
				     client_addr_len);
	} while (true);

	return 0;
}


static struct net_mgmt_event_callback mgmt_cb;

/*  DHCP Client example */
static void handler(struct net_mgmt_event_callback *cb,
		    uint32_t mgmt_event,
		    struct net_if *iface)
{
	int i = 0;

	if (mgmt_event != NET_EVENT_IPV4_ADDR_ADD) {
		return;
	}

	for (i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
		char buf[NET_IPV4_ADDR_LEN];

		if (iface->config.ip.ipv4->unicast[i].addr_type !=
							NET_ADDR_DHCP) {
			continue;
		}

		LOG_INF("Your address: %s",
			log_strdup(net_addr_ntop(AF_INET,
			    &iface->config.ip.ipv4->unicast[i].address.in_addr,
						  buf, sizeof(buf))));
		LOG_INF("Lease time: %u seconds",
			 iface->config.dhcpv4.lease_time);
		LOG_INF("Subnet: %s",
			log_strdup(net_addr_ntop(AF_INET,
				       &iface->config.ip.ipv4->netmask,
				       buf, sizeof(buf))));
		LOG_INF("Router: %s",
			log_strdup(net_addr_ntop(AF_INET,
						 &iface->config.ip.ipv4->gw,
						 buf, sizeof(buf))));
    }
}



void main(void)
{
    int ret;

    if (IS_ENABLED(CONFIG_LOG_BACKEND_RTT)) {
        /* Give RTT log time to be flushed before executing tests */
        k_sleep(K_MSEC(500));
    }

    /*  Get device binding for two distancce sensors  */
    dev = device_get_binding(HC0_LBL);
    dev1 = device_get_binding(HC1_LBL);

    if (dev == NULL) {
        LOG_ERR("Failed to get dev binding");
    }
    else
    {
        LOG_INF("dev is %p, name is %s", dev, dev->name);
    }
    if (dev1 == NULL) {
        LOG_ERR("Failed to get dev1 binding");
    }
    else
    {
        LOG_INF("dev is %p, name is %s", dev1, dev1->name);
    }

    /*  Start LED INIT  */
    gpio1 = device_get_binding(LED0);
    if (gpio1 == NULL) {
        LOG_ERR("Failed to get dev binding");
        return;
    }
    gpio3 = device_get_binding(LED2);
    if (gpio3 == NULL) {
        LOG_ERR("Failed to get dev binding");
        return;
    }
    ret = gpio_pin_configure(gpio1, PIN0, GPIO_OUTPUT_ACTIVE | FLAGS0);
    if (ret < 0) {
        return;
    }
    ret = gpio_pin_configure(gpio1, PIN1, GPIO_OUTPUT_ACTIVE | FLAGS1);
    if (ret < 0) {
        return;
    }
    ret = gpio_pin_configure(gpio3, PIN2, GPIO_OUTPUT_ACTIVE | FLAGS2);
    if (ret < 0) {
        return;
    }

    /*  Start DHCP client   */
    int r = 0;
	struct net_if *iface;
	LOG_INF("Run dhcpv4 client");

	net_mgmt_init_event_callback(&mgmt_cb, handler,
				     NET_EVENT_IPV4_ADDR_ADD);
	net_mgmt_add_event_callback(&mgmt_cb);

	iface = net_if_get_default();
	net_dhcpv4_start(iface);
	
    r = start_coap_server();
	if (r < 0) {
		goto quit;
	}

	k_work_init_delayable(&retransmit_work, retransmit_request);
    
    /*  Start the sensor thread */
    init_sensor_thread();


    /*  Keep on processing requests */
    while (1) {
		r = process_client_request();
		if (r < 0) {
			goto quit;
		}
	}

    LOG_INF("Done");
	return;
quit:
	LOG_INF("Quit");
}
