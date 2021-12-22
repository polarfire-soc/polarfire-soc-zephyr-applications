/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <drivers/entropy.h>
#include <drivers/counter.h>
#include <drivers/gpio.h>
#include "busy_sim.h"
#include <sys/ring_buffer.h>

#define BUFFER_SIZE 32

struct busy_sim_data {
	uint32_t idle_avg;
	uint32_t active_avg;
	uint16_t idle_delta;
	uint16_t active_delta;
	uint32_t us_tick;
	struct counter_alarm_cfg alarm_cfg;
	struct k_work work;
	struct ring_buf rnd_rbuf;
	uint8_t rnd_buf[BUFFER_SIZE];
};

struct busy_sim_config {
	const struct device *entropy;
	const struct device *counter;
	struct gpio_dt_spec pin_spec;
};

#define DT_BUSY_SIM DT_COMPAT_GET_ANY_STATUS_OKAY(vnd_busy_sim)

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(vnd_busy_sim) == 1,
	     "add exactly one vnd,busy-sim node to the devicetree");

static const struct busy_sim_config sim_config = {
	.entropy = DEVICE_DT_GET(DT_CHOSEN(zephyr_entropy)),
	.counter = DEVICE_DT_GET(DT_PHANDLE(DT_BUSY_SIM, counter)),
	.pin_spec = GPIO_DT_SPEC_GET_OR(DT_BUSY_SIM, active_gpios, {0}),
};

static struct busy_sim_data sim_data;
static const struct device *busy_sim_dev = DEVICE_DT_GET_ONE(vnd_busy_sim);

static inline struct busy_sim_data *get_dev_data(const struct device *dev)
{
	return dev->data;
}

static inline const struct busy_sim_config *get_dev_config(const struct device *dev)
{
	return dev->config;
}

static void rng_pool_work_handler(struct k_work *work)
{
	uint8_t *buf;
	uint32_t len;
	struct busy_sim_data *data = get_dev_data(busy_sim_dev);
	const struct busy_sim_config *config = get_dev_config(busy_sim_dev);

	len = ring_buf_put_claim(&data->rnd_rbuf, &buf, BUFFER_SIZE - 1);
	if (len) {
		int err = entropy_get_entropy(config->entropy, buf, len);

		if (err == 0) {
			ring_buf_put_finish(&data->rnd_rbuf, len);
			return;
		}
	}

	k_work_submit(work);
}


static uint32_t get_timeout(bool idle)
{
	struct busy_sim_data *data = get_dev_data(busy_sim_dev);
	uint32_t avg = idle ? data->idle_avg : data->active_avg;
	uint32_t delta = idle ? data->idle_delta : data->active_delta;
	uint16_t rand_val;
	uint32_t len;

	len = ring_buf_get(&data->rnd_rbuf, (uint8_t *)&rand_val, sizeof(rand_val));
	if (len < sizeof(rand_val)) {
		k_work_submit(&data->work);
		rand_val = 0;
	}

	avg *= data->us_tick;
	delta *= data->us_tick;

	return avg - delta + 2 * (rand_val % delta);
}

static void counter_alarm_callback(const struct device *dev,
				   uint8_t chan_id, uint32_t ticks,
				   void *user_data)
{
	int err;
	const struct busy_sim_config *config = get_dev_config(busy_sim_dev);
	struct busy_sim_data *data = get_dev_data(busy_sim_dev);

	data->alarm_cfg.ticks = get_timeout(true);

	if (config->pin_spec.port) {
		err = gpio_pin_set_dt(&config->pin_spec, 1);
		__ASSERT_NO_MSG(err >= 0);
	}

	/* Busy loop */
	k_busy_wait(get_timeout(false) / data->us_tick);

	if (config->pin_spec.port) {
		err = gpio_pin_set_dt(&config->pin_spec, 0);
		__ASSERT_NO_MSG(err >= 0);
	}

	err = counter_set_channel_alarm(config->counter, 0, &data->alarm_cfg);
	__ASSERT_NO_MSG(err == 0);

}

void busy_sim_start(uint32_t active_avg, uint32_t active_delta,
		    uint32_t idle_avg, uint32_t idle_delta)
{
	int err;
	const struct busy_sim_config *config = get_dev_config(busy_sim_dev);
	struct busy_sim_data *data = get_dev_data(busy_sim_dev);

	data->active_avg = active_avg;
	data->active_delta = active_delta;
	data->idle_avg = idle_avg;
	data->idle_delta = idle_delta;

	err = k_work_submit(&data->work);
	__ASSERT_NO_MSG(err >= 0);

	data->alarm_cfg.ticks = counter_us_to_ticks(config->counter, 100);
	err = counter_set_channel_alarm(config->counter, 0, &data->alarm_cfg);
	__ASSERT_NO_MSG(err == 0);

	err = counter_start(config->counter);
	__ASSERT_NO_MSG(err == 0);
}

void busy_sim_stop(void)
{
	int err;
	const struct busy_sim_config *config = get_dev_config(busy_sim_dev);
	struct busy_sim_data *data = get_dev_data(busy_sim_dev);

	k_work_cancel(&data->work);
	err = counter_stop(config->counter);
	__ASSERT_NO_MSG(err == 0);
}

static int busy_sim_init(const struct device *dev)
{
	uint32_t freq;
	const struct busy_sim_config *config = get_dev_config(dev);
	struct busy_sim_data *data = get_dev_data(dev);

	if ((config->pin_spec.port && !device_is_ready(config->pin_spec.port)) ||
	    !device_is_ready(config->counter) || !device_is_ready(config->entropy)) {
		__ASSERT(0, "Devices needed by busy simulator not ready.");
		return -EIO;
	}

	if (config->pin_spec.port) {
		gpio_pin_configure_dt(&config->pin_spec, GPIO_OUTPUT | GPIO_OUTPUT_INIT_LOW);
	}

	freq = counter_get_frequency(config->counter);
	if (freq < 1000000) {
		__ASSERT(0, "Counter device has too low frequency for busy simulator.");
		return -EINVAL;
	}

	k_work_init(&data->work, rng_pool_work_handler);
	ring_buf_init(&data->rnd_rbuf, BUFFER_SIZE, data->rnd_buf);

	data->us_tick = freq / 1000000;
	data->alarm_cfg.callback = counter_alarm_callback;
	data->alarm_cfg.flags = COUNTER_ALARM_CFG_EXPIRE_WHEN_LATE;

	return 0;
}

DEVICE_DT_DEFINE(DT_BUSY_SIM, busy_sim_init, NULL,
	      &sim_data, &sim_config,
	      APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY,
	      NULL);
