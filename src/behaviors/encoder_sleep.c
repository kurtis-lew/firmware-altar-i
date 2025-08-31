/*
 * Copyright (c) 2023 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>

#include "ec11.h"

#define PULL_UPS_ENABLED (GPIO_INPUT | GPIO_ACTIVE_HIGH | GPIO_PULL_UP)
#define PULL_UPS_DISABLED (GPIO_INPUT | GPIO_ACTIVE_HIGH)

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define GET_EC11(node_id) DEVICE_DT_GET(node_id),

static const struct device *ec11_devs[] = {DT_FOREACH_STATUS_OKAY(alps_ec11, GET_EC11)};

static int on_activity_state(const zmk_event_t *eh) {
    struct zmk_activity_state_changed *state_ev = as_zmk_activity_state_changed(eh);

    if (!state_ev) {
        LOG_WRN("NO EVENT, leaving early");
        return 0;
    }

    for (size_t i = 0; i < ARRAY_SIZE(ec11_devs); i++) {
        const struct ec11_config *drv_cfg = ec11_devs[i]->config;

        switch (state_ev->state) {
        case ZMK_ACTIVITY_ACTIVE:
            LOG_DBG("Entering active mode. Re-enabling encoders.");
            if (gpio_pin_configure_dt(&drv_cfg->a, PULL_UPS_ENABLED)) {
                return -EIO;
            }
            if (gpio_pin_configure_dt(&drv_cfg->b, PULL_UPS_ENABLED)) {
                return -EIO;
            }
            break;

#if (CONFIG_ZMK_EC11_AUTO_OFF_IDLE)
        case ZMK_ACTIVITY_IDLE:
#endif // (CONFIG_ZMK_EC11_AUTO_OFF_IDLE)

#if (CONFIG_ZMK_EC11_AUTO_OFF_IDLE || CONFIG_ZMK_EC11_AUTO_OFF_SLEEP)
        case ZMK_ACTIVITY_SLEEP:
            LOG_DBG("Disabling encoders.");
            if (!gpio_pin_get_dt(&drv_cfg->a)) {
                if (gpio_pin_configure_dt(&drv_cfg->a, PULL_UPS_DISABLED)) {
                    LOG_ERR("Failed to disable pin A.");
                    return -EIO;
                }
                LOG_DBG("Disabled pin A.");
            }
            if (!gpio_pin_get_dt(&drv_cfg->b)) {
                if (gpio_pin_configure_dt(&drv_cfg->b, PULL_UPS_DISABLED)) {
                    LOG_ERR("Failed to disable pin B.");
                    return -EIO;
                }
                LOG_DBG("Disabled pin B.");
            }
            break;
#endif // (CONFIG_ZMK_EC11_AUTO_OFF_IDLE || CONFIG_ZMK_EC11_AUTO_OFF_SLEEP)
        default:
            LOG_WRN("Unhandled activity state: %d", state_ev->state);
            return -EINVAL;
        }
    }

    return 0;
}

ZMK_LISTENER(zmk_encoder_sleep, on_activity_state);
ZMK_SUBSCRIPTION(zmk_encoder_sleep, zmk_activity_state_changed);