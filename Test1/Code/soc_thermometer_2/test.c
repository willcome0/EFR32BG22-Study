#include <stdbool.h>
#include "em_common.h"
#include "sl_status.h"
#include "sl_simple_button_instances.h"
#include "sl_simple_led_instances.h"
#include "sl_simple_timer.h"
#include "sl_app_log.h"
#include "sl_app_assert.h"
#include "sl_bluetooth.h"
#include "gatt_db.h"
#ifdef SL_COMPONENT_CATALOG_PRESENT
#include "sl_component_catalog.h"
#endif // SL_COMPONENT_CATALOG_PRESENT
#ifdef SL_CATALOG_CLI_PRESENT
#include "sl_cli.h"
#endif // SL_CATALOG_CLI_PRESENT
#include "sl_sensor_rht.h"
#include "sl_health_thermometer.h"
#include "app.h"
#include "test.h"

static sl_simple_timer_t timer;
static void timer_cb(sl_simple_timer_t *timer, void *data)
{
    (void)data;
    (void)timer;
    int32_t temp = 0;
    uint32_t humidity = 0;
    sl_app_log("timer\n");
    if (SL_STATUS_OK == sl_sensor_rht_get(&humidity, &temp)) {
        uint8_t t_buf[6];
        sprintf(t_buf, "%2.1f C", (float)temp / 1000);
        sl_app_log("timer read ok: %s\n", t_buf);
        sl_bt_gatt_server_send_notification(1, gattdb_temperature, 6, t_buf);

        if (temp >= 25000) {
            uint8_t a_buf[26];
            sprintf(a_buf, "Temperature Alarm! %2.1f C", (float)temp / 1000);
            sl_app_log("timer alarm ok: %s\n", a_buf);
            sl_bt_gatt_server_send_notification(1, gattdb_alarm, 26, t_buf);
        }
    }

}

void test(sl_bt_msg_t *evt)
{
    sl_bt_evt_gatt_server_characteristic_status_t *status = &evt->data.evt_gatt_server_characteristic_status;
    switch (SL_BT_MSG_ID(evt->header))
    {
        case sl_bt_evt_gatt_server_user_read_request_id: {
            switch (status->characteristic)
            {
                case gattdb_led: {
                    uint8_t value = sl_led_get_state(&sl_led_led0);
                    sl_bt_gatt_server_send_user_read_response(1,
                                    status->characteristic, 0, 1, &value, NULL);
                    break;
                }
                case gattdb_temperature: {
                    int32_t temp = 0;
                    uint32_t humidity = 0;
                    uint8_t buf[6] = "ERROR.";
                    if (SL_STATUS_OK == sl_sensor_rht_get(&humidity, &temp)) {
                        sprintf(buf, "%2.1f C", (float)temp / 1000);
                        sl_app_log("read ok: %s\n", buf);
                    }
                    sl_bt_gatt_server_send_user_read_response(1,
                                    status->characteristic, 0, 6, &buf, NULL);
                    break;
                }
                default: {
                    return;
                }
            }

            break;
        }

        case sl_bt_evt_gatt_server_user_write_request_id: {
            uint8array *value = &evt->data.evt_gatt_server_user_write_request.value;

            switch (status->characteristic)
            {
                case gattdb_led: {
                    if (value->len != 1) {
                        return;
                    }
                    if (value->data[0] == 0) {
                        sl_led_turn_off(&sl_led_led0);
                    }
                    else {
                        sl_led_turn_on(&sl_led_led0);
                    }
                    break;
                }

                default: {
                    return;
                }
            }
            sl_bt_gatt_server_send_user_write_response(1, status->characteristic, 0);
            break;
        }

        case sl_bt_evt_gatt_server_characteristic_status_id: {
            switch (status->characteristic)
            {
                case gattdb_alarm: {
                    if (gatt_server_client_config == status->status_flags) {
                        if (gatt_disable != status->client_config_flags) {
                            sl_simple_timer_start(&timer, 1000, timer_cb, NULL, true);
                        }
                        else {
                            sl_simple_timer_stop(&timer);
                        }
                    }
                    break;
                }
            }
            break;
        }
    }
}
