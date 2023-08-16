/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/gpio.h>
/*Refer to zephyr/drivers/sensor/lis2dh for further information on sensor attribute usage*/
#if IS_ENABLED(CONFIG_LIS2DH_ACCEL_RANGE_2G)
#define ACCEL_RANGE_MAX_M_S2 19.6133
#elif IS_ENABLED(CONFIG_LIS2DH_ACCEL_RANGE_4G)
#define ACCEL_RANGE_MAX_M_S2 39.2266
#elif IS_ENABLED(cONFIG_LIS2DH_ACCEL_RANGE_8G)
#define ACCEL_RANGE_MAX_M_S2 78.4532
#elif IS_ENABLED(cONFIG_LIS2DH_ACCEL_RANGE_16G)
#define ACCEL_RANGE_MAX_M_S2 156.9064
#endif

#define LIS2DH_ACTIVITY_MAX 127
#if IS_ENABLED(CONFIG_LIS2DH_ODR_1)
#define LIS2DH_ACTIVITY_MAX_S 127
#define LIS2DH_ODR 1
#elif IS_ENABLED(CONFIG_LIS2DH_ODR_2)
#define LIS2DH_ACTIVITY_MAX_S 12.7
#define LIS2DH_ODR 10
#elif IS_ENABLED(CONFIG_LIS2DH_ODR_3)
#define LIS2DH_ACTIVITY_MAX_S 5.08
#define LIS2DH_ODR 25
#elif IS_ENABLED(CONFIG_LIS2DH_ODR_4)
#define LIS2DH_ACTIVITY_MAX_S 2.54
#define LIS2DH_ODR 50
#elif IS_ENABLED(CONFIG_LIS2DH_ODR_5)
#define LIS2DH_ACTIVITY_MAX_S 1.27
#define LIS2DH_ODR 100
#elif IS_ENABLED(CONFIG_LIS2DH_ODR_6)
#define LIS2DH_ACTIVITY_MAX_S 0.635
#define LIS2DH_ODR 200
#elif IS_ENABLED(CONFIG_LIS2DH_ODR_7)
#define LIS2DH_ACTIVITY_MAX_S 0.3175
#define LIS2DH_ODR 400
#elif IS_ENABLED(CONFIG_LIS2DH_ODR_8)
#define LIS2DH_ACTIVITY_MAX_S 0.079375
#define LIS2DH_ODR 1600
#elif IS_ENABLED(CONFIG_LIS2DH_ODR_9_LOW)
#define LIS2DH_ACTIVITY_MAX_S 0.1016
#define LIS2DH_ODR 1250
#elif IS_ENABLED(CONFIG_LIS2DH_ODR_9_NORMAL)
#define LIS2DH_ACTIVITY_MAX_S 0.0254
#define LIS2DH_ODR 5000
#endif

static void fetch_and_display(const struct device *sensor)
{
	static unsigned int count;
	struct sensor_value accel[3];
	struct sensor_value temperature;
	const char *overrun = "";
	int rc = sensor_sample_fetch(sensor);

	++count;
	if (rc == -EBADMSG) {
		/* Sample overrun.  Ignore in polled mode. */
		if (IS_ENABLED(CONFIG_LIS2DH_TRIGGER)) {
			overrun = "[OVERRUN] ";
		}
		rc = 0;
	}
	if (rc == 0) {
		rc = sensor_channel_get(sensor,
					SENSOR_CHAN_ACCEL_XYZ,
					accel);
	}
	if (rc < 0) {
		printf("ERROR: Update failed: %d\n", rc);
	} else {
		printf("#%u @ %u ms: %sx %lf , y %lf , z %lf",
		       count, k_uptime_get_32(), overrun,
		       sensor_value_to_double(&accel[0]),
		       sensor_value_to_double(&accel[1]),
		       sensor_value_to_double(&accel[2]));
	}

	if (IS_ENABLED(CONFIG_LIS2DH_MEASURE_TEMPERATURE)) {
		if (rc == 0) {
			rc = sensor_channel_get(sensor, SENSOR_CHAN_DIE_TEMP, &temperature);
			if (rc < 0) {
				printf("\nERROR: Unable to read temperature:%d\n", rc);
			} else {
				printf(", t %f\n", sensor_value_to_double(&temperature));
			}
		}

	} else {
		printf("\n");
	}
}

#ifdef CONFIG_LIS2DH_TRIGGER
#ifndef CONFIG_LIS2DH_ODR_RUNTIME
struct k_timer wait_timer;
struct k_work re_enable_interrupt_work_data;
static void trigger_handler(const struct device *dev, const struct sensor_trigger *trig);

void re_enable_interrupt_work_handler(struct k_work *work){
	const struct device *const sensor = DEVICE_DT_GET_ANY(st_lis2dh);
	struct sensor_trigger trig = {.chan = SENSOR_CHAN_ACCEL_XYZ, .type=SENSOR_TRIG_DELTA};
	int rc = sensor_trigger_set(sensor, &trig, trigger_handler);
	if (rc != 0) {
		printf("Failed to reset trigger: %d\n", rc);
		return;
	}
}
K_WORK_DEFINE(re_enable_interrupt_work_data, re_enable_interrupt_work_handler);

void re_enable_interrupt(struct k_timer *timer_id){
	k_work_submit(&re_enable_interrupt_work_data);
}
K_TIMER_DEFINE(wait_timer, re_enable_interrupt, NULL);


/*threshold acceleration to trigger, in units m/s^2, accelerometer can only take a 7 bit value*/
int accelerometer_threshold_set(const struct device *dev, double threshold)
{
	int err;
	if (threshold > ACCEL_RANGE_MAX_M_S2){
		printf("Threshold %lf exceeds maximum %lf, capping it\n", threshold, ACCEL_RANGE_MAX_M_S2);
		threshold = ACCEL_RANGE_MAX_M_S2;
	}
	if (threshold < 0){
		printf("Threshold %lf cannot be less than 0, limiting it to 0\n", threshold);
		threshold = 0;
	}
	threshold *= 1000000;
	const struct sensor_value data = {
		.val1 = (unsigned long)threshold/1000000,
		.val2 = (unsigned long)threshold % 1000000
	};


	err = sensor_attr_set(dev,
		SENSOR_CHAN_ACCEL_XYZ,
		SENSOR_ATTR_SLOPE_TH,
		&data);
	if (err) {
		printf("Failed to set accelerometer threshold value\n");
		printf("Device: %s, error: %d\n",
			dev->name, err);
		return err;
	}
	return 0;
}

/*Duration dependent on ODR, 7 bit value*/
int duration_time_set(const struct device *dev, double duration)
{
	int err, inact_time_decimal;

	if (duration > LIS2DH_ACTIVITY_MAX_S || duration < 0) {
		printf("Invalid timeout value\n");
		return -ENOTSUP;
	}

	duration = LIS2DH_ODR*duration;
	inact_time_decimal = (int) (duration + 0.5);
	inact_time_decimal = MIN(inact_time_decimal, LIS2DH_ACTIVITY_MAX);
	inact_time_decimal = MAX(inact_time_decimal, 0);
	printf("Decimal Value for act duration %d\n", inact_time_decimal);
	const struct sensor_value data = {
		.val1 = inact_time_decimal
	};

	err = sensor_attr_set(dev,
			      SENSOR_CHAN_ACCEL_XYZ,
			      SENSOR_ATTR_SLOPE_DUR,
			      &data);
	if (err) {
		printf("Failed to set accelerometer inactivity timeout value\n");
		printf("Device: %s, error: %d\n", dev->name, err);
		return err;
	}
	return 0;
}
#endif
#endif
static void trigger_handler(const struct device *dev,
			    const struct sensor_trigger *trig)
{
	fetch_and_display(dev);
#ifndef CONFIG_LIS2DH_ODR_RUNTIME
	struct sensor_trigger trig1 = *trig;
	trig1.type=SENSOR_TRIG_DELTA;
	int rc = sensor_trigger_set(dev, &trig1, NULL);
	if (rc != 0) {
		printf("Failed to clear trigger: %d\n", rc);
		return;
	}
	k_timer_start(&wait_timer, K_MSEC(1000/LIS2DH_ODR), K_NO_WAIT);
#endif
}
void main(void)
{
	const struct device *const sensor = DEVICE_DT_GET_ANY(st_lis2dh);

	if (sensor == NULL) {
		printf("No device found\n");
		return;
	}
	if (!device_is_ready(sensor)) {
		printf("Device %s is not ready\n", sensor->name);
		return;
	}

#if CONFIG_LIS2DH_TRIGGER
	{
		struct sensor_trigger trig;
		int rc;

		trig.type = SENSOR_TRIG_DATA_READY;
		trig.chan = SENSOR_CHAN_ACCEL_XYZ;

		if (IS_ENABLED(CONFIG_LIS2DH_ODR_RUNTIME)) {
			//Sensor interrupts host for frequency set at ODR Hz
			struct sensor_value odr = {
				.val1 = 1,
			};

			rc = sensor_attr_set(sensor, trig.chan,
					     SENSOR_ATTR_SAMPLING_FREQUENCY,
					     &odr);
			if (rc != 0) {
				printf("Failed to set odr: %d\n", rc);
				return;
			}
			printf("Sampling at %u Hz\n", odr.val1);
		} else{
			//Sensor interrupts host when motion is above set threshold for a specified amount of time
			//Using 6D motion detection, interrupt trigger remains high for ODR duration
			rc = accelerometer_threshold_set(sensor, 10);
			if (rc != 0){
				printf("Failed to set accelerometer threshold: %d\n", rc);
			}
			rc = duration_time_set(sensor, 0.1);
			if (rc != 0){
				printf("Failed to set accelerometer threshold: %d\n", rc);
			}
			trig.type=SENSOR_TRIG_DELTA;
		}
		rc = sensor_trigger_set(sensor, &trig, trigger_handler);
		if (rc != 0) {
			printf("Failed to set trigger: %d\n", rc);
			return;
		}
		printf("Waiting for triggers\n");
		while (true) {
			k_sleep(K_MSEC(2000));
		}
	}
#else /* CONFIG_LIS2DH_TRIGGER */
	printf("Polling at 0.5 Hz\n");
	while (true) {
		fetch_and_display(sensor);
		k_sleep(K_MSEC(2000));
	}
#endif /* CONFIG_LIS2DH_TRIGGER */
}
