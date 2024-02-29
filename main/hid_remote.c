#include <esp_gatt_defs.h>
#include <esp_log.h>
#include <stdint.h>
#include <string.h>
/* BLE */
#include <esp_bt.h>
#include <esp_bt_defs.h>
#include <esp_bt_device.h>
#include <esp_bt_main.h>
#include <esp_event.h>
#include <esp_gap_ble_api.h>
#include <esp_gatt_common_api.h>
#include <esp_gatts_api.h>
#include <soc/soc_caps.h>

#include "tasks.h"

static const char *TAG = "ble_task";
static const char *device_name = "ESP32-C3";
static uint8_t hid_report_descriptor[] = {
    0x05, 0x0c, 0x09, 0x01, 0xa1, 0x01, 0x85, 0x01, 0x19, 0x00, 0x2a, 0x9c,
    0x02, 0x15, 0x00, 0x26, 0x9c, 0x02, 0x95, 0x01, 0x75, 0x10, 0x81, 0x00,
    0x09, 0x02, 0xa1, 0x02, 0x05, 0x09, 0x19, 0x01, 0x29, 0x0a, 0x15, 0x01,
    0x25, 0x0a, 0x95, 0x01, 0x75, 0x08, 0x81, 0x40, 0xc0, 0xc0, 0x06, 0x01,
    0xff, 0x09, 0x01, 0xa1, 0x02, 0x85, 0x05, 0x09, 0x14, 0x75, 0x08, 0x95,
    0x14, 0x15, 0x80, 0x25, 0x7f, 0x81, 0x22, 0x85, 0x04, 0x09, 0x04, 0x75,
    0x08, 0x95, 0x01, 0x91, 0x02, 0xc0, 0x05, 0x01, 0x09, 0x06, 0xa1, 0x01,
    0x85, 0x0a, 0x75, 0x01, 0x95, 0x08, 0x05, 0x07, 0x19, 0xe0, 0x29, 0xe7,
    0x15, 0x00, 0x25, 0x01, 0x81, 0x02, 0x95, 0x01, 0x75, 0x08, 0x81, 0x01,
    0x95, 0x05, 0x75, 0x01, 0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x91, 0x02,
    0x95, 0x01, 0x75, 0x03, 0x91, 0x01, 0x95, 0x06, 0x75, 0x08, 0x15, 0x00,
    0x26, 0xff, 0x00, 0x05, 0x07, 0x19, 0x00, 0x29, 0xff, 0x81, 0x00, 0xc0,
};

static uint8_t addr[] = {0xc0, 0xde, 0x52, 0x00, 0x00, 0x03};
static bool whl_set = false;
static uint8_t peer_addr[6] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
static uint8_t manufacturerData[] = {0x46, 0x00, 0x46, 0xFA, 0xC1, 0x69, 0x04,
                                     0xC8, 0x38, 0xFF, 0xFF, 0xFF, 0xFF};
static uint8_t uuid_report[] = {0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
                                0x00, 0x10, 0x00, 0x00, 0x12, 0x18, 0x00, 0x00};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0xA0,
    .adv_int_max = 0xA0,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_RANDOM,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static esp_ble_adv_data_t adv_data_poweron = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = false,
    .appearance = ESP_BLE_APPEARANCE_HID_KEYBOARD,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(uuid_report),
    .p_service_uuid = &uuid_report[0],
    .flag = ESP_BLE_ADV_FLAG_LIMIT_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT};

static esp_gatt_srvc_id_t report_service_id = {
    .id =
        {
            .uuid =
                {
                    .len = ESP_UUID_LEN_16,
                    .uuid.uuid16 = 0x1812,
                },
            .inst_id = 0,
        },
    .is_primary = true};
static esp_gatt_if_t gatts_if_handle = 0;
static uint16_t report_service_handle = 0;
static esp_bt_uuid_t hid_info_uuid = {
    .len = ESP_UUID_LEN_16, .uuid = {.uuid16 = ESP_GATT_UUID_HID_INFORMATION}};
static esp_attr_control_t auto_resp_ctrl = {.auto_rsp = ESP_GATT_AUTO_RSP};
static uint8_t hid_info_data[4] = {0x11, 0x1, 0x0, 0x1};
static esp_attr_value_t hid_info_attr_value = {
    .attr_max_len = 4,
    .attr_len = 4,
    .attr_value = hid_info_data,
};
static esp_bt_uuid_t hid_report_map_uuid = {
    .len = ESP_UUID_LEN_16, .uuid = {.uuid16 = ESP_GATT_UUID_HID_REPORT_MAP}};
static esp_attr_value_t hid_report_descriptor_attr_value = {
    .attr_max_len = sizeof(hid_report_descriptor),
    .attr_len = sizeof(hid_report_descriptor),
    .attr_value = hid_report_descriptor,
};
static esp_bt_uuid_t hid_control_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = ESP_GATT_UUID_HID_CONTROL_POINT}};
static uint8_t hid_control_data[1] = {0x0};
static esp_attr_value_t hid_control_attr_value = {
    .attr_max_len = 1,
    .attr_len = 1,
    .attr_value = hid_control_data,
};
static esp_bt_uuid_t hid_report_uuid = {
    .len = ESP_UUID_LEN_16, .uuid = {.uuid16 = ESP_GATT_UUID_HID_REPORT}};
static esp_attr_control_t app_resp_ctrl = {.auto_rsp = ESP_GATT_RSP_BY_APP};
static uint16_t hid_report_handle = ESP_GATT_INVALID_HANDLE;
static esp_bt_uuid_t hid_report_cccd_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG}};
static uint8_t hid_report_cccd_data[2] = {0x00, 0x00};
static esp_attr_value_t hid_report_cccd_attr_value = {
    .attr_max_len = 2,
    .attr_len = 2,
    .attr_value = hid_report_cccd_data,
};
static esp_bt_uuid_t hid_report_reference_uuid = {
    .len = ESP_UUID_LEN_16, .uuid = {.uuid16 = ESP_GATT_UUID_RPT_REF_DESCR}};
static uint8_t hid_report_reference_data[2] = {0x0a, 0x01};
static esp_attr_value_t hid_report_reference_attr_value = {
    .attr_max_len = 2,
    .attr_len = 2,
    .attr_value = hid_report_reference_data,
};
static esp_bt_uuid_t hid_proto_mode_uuid = {
    .len = ESP_UUID_LEN_16, .uuid = {.uuid16 = ESP_GATT_UUID_HID_PROTO_MODE}};
static uint8_t hid_proto_mode_data[1] = {0x01};
static esp_attr_value_t hid_proto_mode_attr_value = {
    .attr_max_len = 1,
    .attr_len = 1,
    .attr_value = hid_proto_mode_data,
};
static uint16_t cccd_handle = ESP_GATT_INVALID_HANDLE;
static uint16_t connect_id = UINT16_MAX;
static const int CONNECTED_BIT = BIT0;
static EventGroupHandle_t s_ble_event_group;

static uint8_t key_down_data[7] = {0x00, 0x00, 0x66, 0x00, 0x00, 0x00, 0x00};
static uint8_t key_up_data[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static esp_ble_auth_req_t auth_req =
    ESP_LE_AUTH_REQ_SC_MITM_BOND;  // bonding with peer device after
                                   // authentication
static esp_ble_io_cap_t iocap =
    ESP_IO_CAP_NONE;           // set the IO capability to No output No input
static uint8_t key_size = 16;  // the key size should be 7~16 bytes
static uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
static uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
static uint32_t passkey = 123456;
static uint8_t auth_option = ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_DISABLE;
static uint8_t oob_support = ESP_BLE_OOB_DISABLE;
esp_gatts_attr_db_t a;

static void gap_event_handler(esp_gap_ble_cb_event_t event,
                              esp_ble_gap_cb_param_t *param) {
    if (event == ESP_GAP_BLE_SET_STATIC_RAND_ADDR_EVT) {
        adv_data_poweron.set_scan_rsp = true;
        esp_ble_gap_config_adv_data(&adv_data_poweron);
    } else if (event == ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT) {
        esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &passkey,
                                       sizeof(uint32_t));
        esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req,
                                       sizeof(uint8_t));
        esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap,
                                       sizeof(uint8_t));
        esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size,
                                       sizeof(uint8_t));
        esp_ble_gap_set_security_param(
            ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH, &auth_option,
            sizeof(uint8_t));
        esp_ble_gap_set_security_param(ESP_BLE_SM_OOB_SUPPORT, &oob_support,
                                       sizeof(uint8_t));
        esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key,
                                       sizeof(uint8_t));
        esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key,
                                       sizeof(uint8_t));
        adv_data_poweron.set_scan_rsp = false;
        esp_ble_gap_config_adv_data(&adv_data_poweron);
    } else if (event == ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT) {
        if (!is_ble_connected()) {
            esp_ble_gap_start_advertising(&adv_params);
        }
    } else if (event == ESP_GAP_BLE_SEC_REQ_EVT) {
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
    }
}

void adv_with_mfg_data(bool on) {
    esp_ble_gap_stop_advertising();
    adv_data_poweron.set_scan_rsp = false;
    if (on) {
        adv_data_poweron.p_manufacturer_data = manufacturerData;
        adv_data_poweron.manufacturer_len = sizeof(manufacturerData);
        adv_data_poweron.include_name = false;
    } else {
        adv_data_poweron.include_name = true;
        adv_data_poweron.p_manufacturer_data = NULL;
        adv_data_poweron.manufacturer_len = 0;
    }
    esp_ble_gap_config_adv_data(&adv_data_poweron);
}

static void gatts_event_handler(esp_gatts_cb_event_t event,
                                esp_gatt_if_t gatts_if,
                                esp_ble_gatts_cb_param_t *param) {
    if (event == ESP_GATTS_CONNECT_EVT) {
        esp_ble_gap_stop_advertising();
        ESP_LOGI(TAG, "Connected, address: %x:%x:%x:%x:%x:%x",
                 param->connect.remote_bda[0], param->connect.remote_bda[1],
                 param->connect.remote_bda[2], param->connect.remote_bda[3],
                 param->connect.remote_bda[4], param->connect.remote_bda[5]);
        xEventGroupSetBits(s_ble_event_group, CONNECTED_BIT);
        connect_id = param->connect.conn_id;
        set_connected_state(true);
        if (!whl_set) {
            memcpy(peer_addr, param->connect.remote_bda, sizeof(peer_addr));
            adv_params.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_WLST;
            esp_ble_gap_update_whitelist(true, peer_addr,
                                         BLE_WL_ADDR_TYPE_RANDOM);
            whl_set = true;
        }
    } else if (event == ESP_GATTS_DISCONNECT_EVT) {
        ESP_LOGI(TAG, "Disconnected");
        esp_ble_gap_start_advertising(&adv_params);
        xEventGroupClearBits(s_ble_event_group, CONNECTED_BIT);
        connect_id = UINT16_MAX;
        set_connected_state(false);
    } else if (event == ESP_GATTS_REG_EVT) {
        gatts_if_handle = gatts_if;
        esp_ble_gatts_create_service(gatts_if_handle, &report_service_id,
                                     13);  // 12
    } else if (event == ESP_GATTS_CREATE_EVT) {
        report_service_handle = param->create.service_handle;
        esp_ble_gatts_start_service(report_service_handle);
    } else if (event == ESP_GATTS_START_EVT) {
        // HID Info
        esp_ble_gatts_add_char(report_service_handle, &hid_info_uuid,
                               ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_READ,
                               &hid_info_attr_value, &auto_resp_ctrl);
        // Report Map
        esp_ble_gatts_add_char(report_service_handle, &hid_report_map_uuid,
                               ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_READ,
                               &hid_report_descriptor_attr_value,
                               &auto_resp_ctrl);
        // HID Control
        esp_ble_gatts_add_char(report_service_handle, &hid_control_uuid,
                               ESP_GATT_PERM_WRITE,
                               ESP_GATT_CHAR_PROP_BIT_WRITE_NR,
                               &hid_control_attr_value, &auto_resp_ctrl);
        // Report
        esp_ble_gatts_add_char(
            report_service_handle, &hid_report_uuid, ESP_GATT_PERM_READ,
            ESP_GATT_CHAR_PROP_BIT_NOTIFY, NULL, &app_resp_ctrl);
        esp_ble_gatts_add_char_descr(
            report_service_handle, &hid_report_cccd_uuid,
            ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            &hid_report_cccd_attr_value, &auto_resp_ctrl);
        esp_ble_gatts_add_char_descr(
            report_service_handle, &hid_report_reference_uuid,
            ESP_GATT_PERM_READ, &hid_report_reference_attr_value,
            &auto_resp_ctrl);
        // Protocol Mode
        esp_ble_gatts_add_char(
            report_service_handle, &hid_proto_mode_uuid,
            ESP_GATT_PERM_WRITE | ESP_GATT_PERM_READ,
            ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE_NR,
            &hid_proto_mode_attr_value, &auto_resp_ctrl);
    } else if (event == ESP_GATTS_ADD_CHAR_EVT) {
        if (param->add_char.char_uuid.uuid.uuid16 == ESP_GATT_UUID_HID_REPORT) {
            hid_report_handle = param->add_char.attr_handle;
        }
    } else if (event == ESP_GATTS_READ_EVT) {
        if (param->read.handle == hid_report_handle) {
            esp_gatt_rsp_t rsp;
            memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
            rsp.handle = param->read.handle;
            rsp.attr_value.handle = param->read.handle;
            rsp.attr_value.len = sizeof(key_up_data);
            memcpy(rsp.attr_value.value, key_up_data, sizeof(key_up_data));
            esp_ble_gatts_send_response(gatts_if_handle, param->read.conn_id,
                                        param->read.trans_id, ESP_GATT_OK,
                                        &rsp);
        }
    } else if (event == ESP_GATTS_ADD_CHAR_DESCR_EVT) {
        if (param->add_char_descr.descr_uuid.uuid.uuid16 ==
            ESP_GATT_UUID_CHAR_CLIENT_CONFIG) {
            cccd_handle = param->add_char_descr.attr_handle;
        }
    }
}

bool is_ble_connected(void) {
    return connect_id != UINT16_MAX && ((xEventGroupGetBits(s_ble_event_group) &
                                         CONNECTED_BIT) == CONNECTED_BIT);
}

void send_power_key(bool is_down) {
    if (is_down) {
        esp_ble_gatts_send_indicate(gatts_if_handle, connect_id,
                                    hid_report_handle, sizeof(key_down_data),
                                    key_down_data, false);
    } else {
        esp_ble_gatts_send_indicate(gatts_if_handle, connect_id,
                                    hid_report_handle, sizeof(key_up_data),
                                    key_up_data, false);
    }
}

void init_ble(void) {
    s_ble_event_group = xEventGroupCreate();
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    esp_bluedroid_init_with_cfg(&bluedroid_cfg);
    esp_bluedroid_enable();
    esp_ble_gap_register_callback(gap_event_handler);
    esp_ble_gatts_register_callback(gatts_event_handler);
    esp_bt_dev_set_device_name(device_name);
    esp_ble_gap_set_rand_addr(addr);
    esp_ble_gatts_app_register(ESP_GATT_UUID_HID_SVC);
}