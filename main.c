/**
 * Copyright (c) 2014 - 2020, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/** @file
 *
 * @defgroup ble_sdk_uart_over_ble_main main.c
 * @{
 * @ingroup  ble_sdk_app_nus_eval
 * @brief    UART over BLE application main file.
 *
 * This file contains the source code for a sample application that uses the Nordic UART service.
 * This application uses the @ref srvlib_conn_params module.
 */
#include "bmp280.h"
#include "ATH20.h"
#include <stdint.h>
#include <string.h>
#include "nordic_common.h"
#include "nrf.h"
#include "ble_hci.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdh_ble.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "app_timer.h"
#include "ble_nus.h"
#include "app_uart.h"
#include "app_util_platform.h"
#include "bsp_btn_ble.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_drv_saadc.h"
#include "nrf_delay.h"
#if defined (UART_PRESENT)
#include "nrf_uart.h"
#endif
#if defined (UARTE_PRESENT)
#include "nrf_uarte.h"
#endif

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "NanoEdgeAI_stai.h"
#include "knowledge_stai.h"
float input_user_buffer_stai[DATA_INPUT_USER_STAI * AXIS_NUMBER_STAI]; // Buffer of input values
float output_class_buffer_stai[CLASS_NUMBER_STAI]; // Buffer of class probabilities


#define APP_BLE_CONN_CFG_TAG            1                                           /**< A tag identifying the SoftDevice BLE configuration. */

#define DEVICE_NAME                     "Nordic_UART"                               /**< Name of device. Will be included in the advertising data. */
#define NUS_SERVICE_UUID_TYPE           BLE_UUID_TYPE_VENDOR_BEGIN                  /**< UUID type for the Nordic UART Service (vendor specific). */

#define APP_BLE_OBSERVER_PRIO           3                                           /**< Application's BLE observer priority. You shouldn't need to modify this value. */

#define APP_ADV_INTERVAL                64                                          /**< The advertising interval (in units of 0.625 ms. This value corresponds to 40 ms). */

#define APP_ADV_DURATION                0                                       /**< The advertising duration (180 seconds) in units of 10 milliseconds. */

#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(8, UNIT_1_25_MS)             /**< Minimum acceptable connection interval (20 ms), Connection interval uses 1.25 ms units. */
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(10, UNIT_1_25_MS)             /**< Maximum acceptable connection interval (75 ms), Connection interval uses 1.25 ms units. */
#define SLAVE_LATENCY                   0                                           /**< Slave latency. */
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(8000, UNIT_10_MS)             /**< Connection supervisory timeout (4 seconds), Supervision Timeout uses 10 ms units. */
#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000)                       /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(30000)                      /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT    3                                           /**< Number of attempts before giving up the connection parameter negotiation. */

#define DEAD_BEEF                       0xDEADBEEF                                  /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

#define UART_TX_BUF_SIZE                256                                         /**< UART TX buffer size. */
#define UART_RX_BUF_SIZE                256                                         /**< UART RX buffer size. */


BLE_NUS_DEF(m_nus, NRF_SDH_BLE_TOTAL_LINK_COUNT);  //声明服务的观察者函数                   /**< BLE NUS service instance. */
//下面三个观察者函数必须有
NRF_BLE_GATT_DEF(m_gatt);                                                           /**< GATT module instance. */
NRF_BLE_QWR_DEF(m_qwr);                                                             /**< Context for the Queued Write module.*/
BLE_ADVERTISING_DEF(m_advertising);                                                 /**< Advertising module instance. */

static uint16_t   m_conn_handle          = BLE_CONN_HANDLE_INVALID;                 /**< Handle of the current connection. */
static uint16_t   m_ble_nus_max_data_len = BLE_GATT_ATT_MTU_DEFAULT - 3;            /**< Maximum length of data (in bytes) that can be transmitted to the peer by the Nordic UART service module. */
static ble_uuid_t m_adv_uuids[]          =                                          /**< Universally unique service identifier. */
{
    {BLE_UUID_NUS_SERVICE, NUS_SERVICE_UUID_TYPE}
};


/**@brief Function for assert macro callback.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyse
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in] line_num    Line number of the failing ASSERT call.
 * @param[in] p_file_name File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}

/**@brief Function for initializing the timer module.
 */
static void timers_init(void)
{
    ret_code_t err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for the GAP initialization.
 *
 * @details This function will set up all the necessary GAP (Generic Access Profile) parameters of
 *          the device. It also sets the permissions and appearance.
 */
static void gap_params_init(void)
{
    uint32_t                err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *) DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
                                      
}


/**@brief Function for handling Queued Write Module errors.
 *
 * @details A pointer to this function will be passed to each service which may need to inform the
 *          application about an error.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
static void nrf_qwr_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for handling the data from the Nordic UART Service.接收蓝牙数据在PC机上打印
 *
 * @details This function will process the data received from the Nordic UART BLE Service and send
 *          it to the UART module.
 *
 * @param[in] p_evt       Nordic UART Service event.
 */
/**@snippet [Handling the data received over BLE] */
static void nus_data_handler(ble_nus_evt_t * p_evt)
{

    if (p_evt->type == BLE_NUS_EVT_RX_DATA)//主机写从机，触发RX接收数据事件
    {
        uint32_t err_code;

        NRF_LOG_DEBUG("Received data from BLE NUS. Writing data on UART.");
        NRF_LOG_HEXDUMP_DEBUG(p_evt->params.rx_data.p_data, p_evt->params.rx_data.length);

        for (uint32_t i = 0; i < p_evt->params.rx_data.length; i++)
        {
            do
            {
                err_code = app_uart_put(p_evt->params.rx_data.p_data[i]);//PC机打印，主机发过来的数据
                if ((err_code != NRF_SUCCESS) && (err_code != NRF_ERROR_BUSY))
                {
                    NRF_LOG_ERROR("Failed receiving NUS message. Error 0x%x. ", err_code);
                    APP_ERROR_CHECK(err_code);
                }
            } while (err_code == NRF_ERROR_BUSY);
        }
        if (p_evt->params.rx_data.p_data[p_evt->params.rx_data.length - 1] == '\r')
        {
            while (app_uart_put('\n') == NRF_ERROR_BUSY);
        }
    }

}
/**@snippet [Handling the data received over BLE] */


/**@brief Function for initializing services that will be used by the application.蓝牙服务初始化
 */
static void services_init(void)
{
    uint32_t           err_code;
    ble_nus_init_t     nus_init;
    nrf_ble_qwr_init_t qwr_init = {0};

    // Initialize Queued Write Module.
    qwr_init.error_handler = nrf_qwr_error_handler;

    err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
    APP_ERROR_CHECK(err_code);

    // Initialize NUS.
    memset(&nus_init, 0, sizeof(nus_init));

    nus_init.data_handler = nus_data_handler;

    err_code = ble_nus_init(&m_nus, &nus_init);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling an event from the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters Module
 *          which are passed to the application.
 *
 * @note All this function does is to disconnect. This could have been done by simply setting
 *       the disconnect_on_fail config parameter, but instead we use the event handler
 *       mechanism to demonstrate its use.
 *
 * @param[in] p_evt  Event received from the Connection Parameters Module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    uint32_t err_code;

    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        APP_ERROR_CHECK(err_code);
    }
}


/**@brief Function for handling errors from the Connection Parameters module.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for initializing the Connection Parameters module.
 */
static void conn_params_init(void)
{
    uint32_t               err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                  = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for putting the chip into sleep mode.
 *
 * @note This function will not return.
 */
static void sleep_mode_enter(void)
{
    uint32_t err_code = bsp_indication_set(BSP_INDICATE_IDLE);
    APP_ERROR_CHECK(err_code);

    // Prepare wakeup buttons.
    err_code = bsp_btn_ble_sleep_mode_prepare();
    APP_ERROR_CHECK(err_code);

    // Go to system-off mode (this function will not return; wakeup will cause a reset).
    err_code = sd_power_system_off();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the application.
 *
 * @param[in] ble_adv_evt  Advertising event.
 */
static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    uint32_t err_code;

    switch (ble_adv_evt)
    {
        case BLE_ADV_EVT_FAST:
            err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
            APP_ERROR_CHECK(err_code);
            break;
        case BLE_ADV_EVT_IDLE:
            sleep_mode_enter();
            break;
        default:
            break;
    }
}


/**@brief Function for handling BLE events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 * @param[in]   p_context   Unused.
 */
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
    uint32_t err_code;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            NRF_LOG_INFO("Connected");
            err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
            APP_ERROR_CHECK(err_code);
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr, m_conn_handle);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            NRF_LOG_INFO("Disconnected");
            // LED indication will be changed when advertising starts.
            m_conn_handle = BLE_CONN_HANDLE_INVALID;
            break;

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
        {
            NRF_LOG_DEBUG("PHY update request.");
            ble_gap_phys_t const phys =
            {
                .rx_phys = BLE_GAP_PHY_AUTO,
                .tx_phys = BLE_GAP_PHY_AUTO,
            };
            err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            APP_ERROR_CHECK(err_code);
        } break;

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            // Pairing not supported
            err_code = sd_ble_gap_sec_params_reply(m_conn_handle, BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            // No system attributes have been stored.
            err_code = sd_ble_gatts_sys_attr_set(m_conn_handle, NULL, 0, 0);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTC_EVT_TIMEOUT:
            // Disconnect on GATT Client timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_TIMEOUT:
            // Disconnect on GATT Server timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        default:
            // No implementation needed.
            break;
    }
}


/**@brief Function for the SoftDevice initialization.
 *
 * @details This function initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    ret_code_t err_code;

    err_code = nrf_sdh_enable_request();
    APP_ERROR_CHECK(err_code);

    // Configure the BLE stack using the default settings.
    // Fetch the start address of the application RAM.
    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    APP_ERROR_CHECK(err_code);

    // Enable BLE stack.
    err_code = nrf_sdh_ble_enable(&ram_start);
    APP_ERROR_CHECK(err_code);

    // Register a handler for BLE events.
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
}


/**@brief Function for handling events from the GATT library. */
void gatt_evt_handler(nrf_ble_gatt_t * p_gatt, nrf_ble_gatt_evt_t const * p_evt)
{
    if ((m_conn_handle == p_evt->conn_handle) && (p_evt->evt_id == NRF_BLE_GATT_EVT_ATT_MTU_UPDATED))
    {
        m_ble_nus_max_data_len = p_evt->params.att_mtu_effective - OPCODE_LENGTH - HANDLE_LENGTH;///247字节
        NRF_LOG_INFO("Data len is set to 0x%X(%d)", m_ble_nus_max_data_len, m_ble_nus_max_data_len);
    }
    NRF_LOG_DEBUG("ATT MTU exchange completed. central 0x%x peripheral 0x%x",
                  p_gatt->att_mtu_desired_central,
                  p_gatt->att_mtu_desired_periph);
}


/**@brief Function for initializing the GATT library. */
void gatt_init(void)
{
    ret_code_t err_code;

    err_code = nrf_ble_gatt_init(&m_gatt, gatt_evt_handler);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_ble_gatt_att_mtu_periph_set(&m_gatt, NRF_SDH_BLE_GATT_MAX_MTU_SIZE);//从机MTU的值
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling events from the BSP module.
 *
 * @param[in]   event   Event generated by button press.
 */
void bsp_event_handler(bsp_event_t event)
{
    uint32_t err_code;
    switch (event)
    {
        case BSP_EVENT_SLEEP:
            sleep_mode_enter();
            break;

        case BSP_EVENT_DISCONNECT:
            err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            if (err_code != NRF_ERROR_INVALID_STATE)
            {
                APP_ERROR_CHECK(err_code);
            }
            break;

        case BSP_EVENT_WHITELIST_OFF:
            if (m_conn_handle == BLE_CONN_HANDLE_INVALID)
            {
                err_code = ble_advertising_restart_without_whitelist(&m_advertising);
                if (err_code != NRF_ERROR_INVALID_STATE)
                {
                    APP_ERROR_CHECK(err_code);
                }
            }
            break;

        default:
            break;
    }
}


/**@brief   Function for handling app_uart events.接收PC机的数据上传主机
 *
 * @details This function will receive a single character from the app_uart module and append it to
 *          a string. The string will be be sent over BLE when the last character received was a
 *          'new line' '\n' (hex 0x0A) or if the string has reached the maximum data length.
 */
/**@snippet [Handling the data received over UART] */
void uart_event_handle(app_uart_evt_t * p_event)
{
//    static uint8_t data_array[BLE_NUS_MAX_DATA_LEN];
//    static uint8_t index = 0;
//    uint32_t       err_code;

//    switch (p_event->evt_type)
//    {
//        case APP_UART_DATA_READY://串口接收中断，PC发数据给从机
//            UNUSED_VARIABLE(app_uart_get(&data_array[index]));
//            index++;

//            if ((data_array[index - 1] == '\n') ||
//                (data_array[index - 1] == '\r') ||
//                (index >= m_ble_nus_max_data_len))//244字节
//            {
//                if (index > 1)
//                {
//                    NRF_LOG_DEBUG("Ready to send data over BLE NUS");
//                    NRF_LOG_HEXDUMP_DEBUG(data_array, index);

//                    do
//                    {
//                        uint16_t length = (uint16_t)index;
//                        err_code = ble_nus_data_send(&m_nus, data_array, &length, m_conn_handle);//从机数据上传函数
//                        if ((err_code != NRF_ERROR_INVALID_STATE) &&
//                            (err_code != NRF_ERROR_RESOURCES) &&
//                            (err_code != NRF_ERROR_NOT_FOUND))
//                        {
//                            APP_ERROR_CHECK(err_code);
//                        }
//                    } while (err_code == NRF_ERROR_RESOURCES);
//                }

//                index = 0;
//            }
//            break;

//        case APP_UART_COMMUNICATION_ERROR:
//            APP_ERROR_HANDLER(p_event->data.error_communication);
//            break;

//        case APP_UART_FIFO_ERROR:
//            APP_ERROR_HANDLER(p_event->data.error_code);
//            break;

//        default:
//            break;
//    }
}
/**@snippet [Handling the data received over UART] */


/**@brief  Function for initializing the UART module.
 */
/**@snippet [UART Initialization] */
static void uart_init(void)
{
    uint32_t                     err_code;
    app_uart_comm_params_t const comm_params =
    {
        .rx_pin_no    = RX_PIN_NUMBER,
        .tx_pin_no    = TX_PIN_NUMBER,
        .rts_pin_no   = RTS_PIN_NUMBER,
        .cts_pin_no   = CTS_PIN_NUMBER,
        .flow_control = APP_UART_FLOW_CONTROL_DISABLED,
        .use_parity   = false,
#if defined (UART_PRESENT)
        .baud_rate    = NRF_UART_BAUDRATE_115200
#else
        .baud_rate    = NRF_UARTE_BAUDRATE_115200
#endif
    };

    APP_UART_FIFO_INIT(&comm_params,
                       UART_RX_BUF_SIZE,
                       UART_TX_BUF_SIZE,
                       uart_event_handle,
                       APP_IRQ_PRIORITY_LOWEST,
                       err_code);
    APP_ERROR_CHECK(err_code);
}
/**@snippet [UART Initialization] */


/**@brief Function for initializing the Advertising functionality.
 */
static void advertising_init(void)
{
    uint32_t               err_code;
    ble_advertising_init_t init;

    memset(&init, 0, sizeof(init));

    init.advdata.name_type          = BLE_ADVDATA_FULL_NAME;
    init.advdata.include_appearance = false;
    init.advdata.flags              = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;

    init.srdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    init.srdata.uuids_complete.p_uuids  = m_adv_uuids;

    init.config.ble_adv_fast_enabled  = true;
    init.config.ble_adv_fast_interval = APP_ADV_INTERVAL;
    init.config.ble_adv_fast_timeout  = APP_ADV_DURATION;
    init.evt_handler = on_adv_evt;

    err_code = ble_advertising_init(&m_advertising, &init);
    APP_ERROR_CHECK(err_code);

    ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);
    
    err_code = sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_ADV,m_advertising.adv_handle,4);
    APP_ERROR_CHECK(err_code);    
}


/**@brief Function for initializing buttons and leds.
 *
 * @param[out] p_erase_bonds  Will be true if the clear bonding button was pressed to wake the application up.
 */
static void buttons_leds_init(bool * p_erase_bonds)
{
    bsp_event_t startup_event;

    uint32_t err_code = bsp_init(BSP_INIT_LEDS | BSP_INIT_BUTTONS, bsp_event_handler);
    APP_ERROR_CHECK(err_code);

    err_code = bsp_btn_ble_init(NULL, &startup_event);
    APP_ERROR_CHECK(err_code);

    *p_erase_bonds = (startup_event == BSP_EVENT_CLEAR_BONDING_DATA);
}


/**@brief Function for initializing the nrf log module.
 */
//static void log_init(void)
//{
//    ret_code_t err_code = NRF_LOG_INIT(NULL);
//    APP_ERROR_CHECK(err_code);

//    NRF_LOG_DEFAULT_BACKENDS_INIT();
//}


/**@brief Function for initializing power management.
 */
//static void power_management_init(void)
//{
//    ret_code_t err_code;
//    err_code = nrf_pwr_mgmt_init();
//    APP_ERROR_CHECK(err_code);
//}


/**@brief Function for handling the idle state (main loop).
 *
 * @details If there is no pending log operation, then sleep until next the next event occurs.
 */
static void idle_state_handle(void)
{
    if (NRF_LOG_PROCESS() == false)
    {
        nrf_pwr_mgmt_run();
    }
}


/**@brief Function for starting advertising.
 */
static void advertising_start(void)
{
    uint32_t err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
    APP_ERROR_CHECK(err_code);
}
//SAADC事件回调函数，因为是堵塞模式，所以不需要事件，这里定义了一个空的事件回调函数
void saadc_callback(nrf_drv_saadc_evt_t const * p_event){}


//初始化SAADC，配置使用的SAADC通道的参数
void saadc_init(void)
{
    ret_code_t err_code;
	  //定义ADC通道配置结构体，并使用单端采样配置宏初始化，
	  //NRF_SAADC_INPUT_AIN2是使用的模拟输入通道

    nrf_saadc_channel_config_t channel_config0 =
        NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NRF_SAADC_INPUT_AIN0);
    nrf_saadc_channel_config_t channel_config1 =
        NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NRF_SAADC_INPUT_AIN1);
    nrf_saadc_channel_config_t channel_config2 =
        NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NRF_SAADC_INPUT_AIN2);  
    nrf_saadc_channel_config_t channel_config3 =
        NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NRF_SAADC_INPUT_AIN3);
    nrf_saadc_channel_config_t channel_config4 =
        NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NRF_SAADC_INPUT_AIN4);
    nrf_saadc_channel_config_t channel_config5 =
        NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NRF_SAADC_INPUT_AIN5);
    nrf_saadc_channel_config_t channel_config6 =
        NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NRF_SAADC_INPUT_AIN6);
    nrf_saadc_channel_config_t channel_config7 =
        NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NRF_SAADC_INPUT_AIN7);    
    //初始化SAADC，注册事件回调函数。
	  err_code = nrf_drv_saadc_init(NULL, saadc_callback);
    APP_ERROR_CHECK(err_code);
    //初始化SAADC通道0
    err_code = nrfx_saadc_channel_init(0, &channel_config0); 
    err_code = nrfx_saadc_channel_init(1, &channel_config1);
    err_code = nrfx_saadc_channel_init(2, &channel_config2);
    err_code = nrfx_saadc_channel_init(3, &channel_config3);
    err_code = nrfx_saadc_channel_init(4, &channel_config4);
    err_code = nrfx_saadc_channel_init(5, &channel_config5);    
    err_code = nrfx_saadc_channel_init(6, &channel_config6);
    err_code = nrfx_saadc_channel_init(7, &channel_config7);

    APP_ERROR_CHECK(err_code);

}

/**@brief Application main function.
 */
uint32_t       err_code;
uint8_t data_array1[60];
uint8_t ave_flag=0;
float temp_val0,temp_val1,temp_val2,temp_val3,temp_val4,temp_val5,temp_val6,temp_val7;
float adc0_val,adc1_val,adc2_val,adc3_val,adc4_val,adc5_val,adc6_val,adc7_val;
uint16_t length1 = 43;
uint16_t id_class_stai = 0;
    bool erase_bonds;
    float AHT10wd=0;uint8_t AHT10sd=0;
    float bmpdqy=0,bmpwd=0,bmphb=0;
	nrf_saadc_value_t  saadc_val0,saadc_val1,saadc_val2,saadc_val3,saadc_val4;
    nrf_saadc_value_t  saadc_val5,saadc_val6,saadc_val7;
void fill_buffer_stai(float sample_buffer_stai[])
{
	sample_buffer_stai[0]=saadc_val0;
    sample_buffer_stai[1]=saadc_val1;
	sample_buffer_stai[2]=saadc_val2;
	sample_buffer_stai[3]=saadc_val3;
	sample_buffer_stai[4]=saadc_val4;
}


int main(void)
{

    // Initialize.
    uart_init();//串口初始化
   // log_init();//log打印初始化
    timers_init();//软件定时器初始化
    buttons_leds_init(&erase_bonds);//按键LED灯初始化
    //power_management_init();//能量管理
    ble_stack_init();//协议栈初始化
    gap_params_init();//GAP初始化
    gatt_init();//GATT初始化
    services_init();//服务初始化
    advertising_init();//广播初始化
    conn_params_init();//连接参数更新初始化
    saadc_init();
    // Start execution.
//    printf("\r\nUART started.\r\n");
//    NRF_LOG_INFO("Debug logging for UART over RTT started.");
    advertising_start();//开始广播
    ATH10Init();
    bmp280Init();
    neai_classification_init_stai(knowledge_stai);
    //bmp280Init(); 
    // Enter main loop.
    while(true)  
    {
       idle_state_handle();

	   
        
//      ATH10ReadData(&AHT10wd,&AHT10sd);
//      nrf_delay_us(500);
//      bmp280GetData(&bmpdqy,&bmpwd,&bmphb);
      if(ave_flag<10)
     {   
         nrfx_saadc_sample_convert(0,&saadc_val0);
         nrfx_saadc_sample_convert(1,&saadc_val1);
         nrfx_saadc_sample_convert(2,&saadc_val2);
		 nrfx_saadc_sample_convert(3,&saadc_val3);
		 nrfx_saadc_sample_convert(4,&saadc_val4);    
		 nrfx_saadc_sample_convert(5,&saadc_val5);
		 nrfx_saadc_sample_convert(6,&saadc_val6);
		 nrfx_saadc_sample_convert(7,&saadc_val7);
         ave_flag++;
         temp_val0+=saadc_val0;
         temp_val1+=saadc_val1;
         temp_val2+=saadc_val2;
         temp_val3+=saadc_val3;
         temp_val4+=saadc_val4;
         temp_val5+=saadc_val5;         
         temp_val6+=saadc_val6; 
         temp_val7+=saadc_val7;         
     }
     if(ave_flag==10)
     {
//         
         adc0_val=temp_val0/ave_flag* 3.6f/4096.0f*1000;
         adc1_val=temp_val1/ave_flag* 3.6f/4096.0f*1000;
         adc2_val=temp_val2/ave_flag* 3.6f/4096.0f*1000;
         adc3_val=temp_val3/ave_flag* 3.6f/4096.0f*1000;        
         adc4_val=temp_val4/ave_flag* 3.6f/4096.0f*1000;
         adc5_val=temp_val5/ave_flag* 3.6f/4096.0f*1000;
         adc6_val=temp_val6/ave_flag* 3.6f/4096.0f*1000;
         adc7_val=temp_val7/ave_flag* 3.6f/4096.0f*1000;
//         data_array1[0]=((int)adc0_val/1000)%10+'0';
//         data_array1[1]='.';
//         data_array1[2]=((int)adc0_val/100)%10+'0';  
//         data_array1[3]=((int)adc0_val/10)%10+'0'; 
//         data_array1[4]=(int)adc0_val%10+'0';       
//         data_array1[5]='\t';
//         data_array1[6]=((int)adc1_val/1000)%10+'0';
//         data_array1[7]='.';         
//         data_array1[8]=((int)adc1_val/100)%10+'0'; 
//         data_array1[9]=((int)adc1_val/10)%10+'0';  
//         data_array1[10]=(int)adc1_val%10+'0';   
//         data_array1[11]='\t';                  
//         data_array1[12]=((int)adc2_val/1000)%10+'0';
//         data_array1[13]='.';         
//         data_array1[14]=((int)adc2_val/100)%10+'0'; 
//         data_array1[15]=((int)adc2_val/10)%10+'0';  
//         data_array1[16]=(int)adc2_val%10+'0';            
//         data_array1[17]='\t';         
//         data_array1[18]=((int)adc3_val/1000)%10+'0';
//         data_array1[19]='.';         
//         data_array1[20]=((int)adc3_val/100)%10+'0'; 
//         data_array1[21]=((int)adc3_val/10)%10+'0';  
//         data_array1[22]=(int)adc3_val%10+'0';            
//         data_array1[23]='\t';         
//         data_array1[24]=((int)adc4_val/1000)%10+'0';
//         data_array1[25]='.';         
//         data_array1[26]=((int)adc4_val/100)%10+'0'; 
//         data_array1[27]=((int)adc4_val/10)%10+'0';  
//         data_array1[28]=(int)adc4_val%10+'0';            
//         data_array1[29]='\t';         
//         data_array1[30]=((int)adc5_val/1000)%10+'0'; 
//         data_array1[31]='.';         
//         data_array1[32]=((int)adc5_val/100)%10+'0'; 
//         data_array1[33]=((int)adc5_val/10)%10+'0';  
//         data_array1[34]=(int)adc5_val%10+'0';            
//         data_array1[35]='\t';         
//         data_array1[36]=((int)adc6_val/1000)%10+'0';
//         data_array1[37]='.';         
//         data_array1[38]=((int)adc6_val/100)%10+'0'; 
//         data_array1[39]=((int)adc6_val/10)%10+'0';  
//         data_array1[40]=(int)adc6_val%10+'0';            
//         data_array1[41]='\t';         
//         data_array1[42]=((int)adc7_val/1000)%10+'0';
//         data_array1[43]='.';         
//         data_array1[44]=((int)adc7_val/100)%10+'0'; 
//         data_array1[45]=((int)adc7_val/10)%10+'0';  
//         data_array1[46]=(int)adc7_val%10+'0';                            
//         data_array1[47]='\r';
//         data_array1[48]='\n';         
       fill_buffer_stai(input_user_buffer_stai);
	   neai_classification_stai(input_user_buffer_stai, output_class_buffer_stai, &id_class_stai);  
         
         data_array1[0]='/';
         data_array1[1]='*';
         data_array1[2]=((int)AHT10wd/1000)%10+'0';  
         data_array1[3]=((int)AHT10wd/100)%10+'0'; 
         data_array1[4]=((int)AHT10wd/10)%10+'0';  
         data_array1[5]=(int)AHT10wd%10+'0';          
         data_array1[6]=',';
         data_array1[7]=((int)AHT10sd/1000)%10+'0';  
         data_array1[8]=((int)AHT10sd/100)%10+'0'; 
         data_array1[9]=((int)AHT10sd/10)%10+'0';  
         data_array1[10]=(int)AHT10sd%10+'0';   
         data_array1[11]=',';                  
         data_array1[12]=((int)bmpdqy/1000)%10+'0';  
         data_array1[13]=((int)bmpdqy/100)%10+'0'; 
         data_array1[14]=((int)bmpdqy/10)%10+'0';  
         data_array1[15]=(int)bmpdqy%10+'0';            
         data_array1[16]=',';         
         data_array1[17]=((int)adc3_val/1000)%10+'0';  
         data_array1[18]=((int)adc3_val/100)%10+'0'; 
         data_array1[19]=((int)adc3_val/10)%10+'0';  
         data_array1[20]=(int)adc3_val%10+'0';            
         data_array1[21]=',';         
         data_array1[22]=((int)adc4_val/1000)%10+'0';  
         data_array1[23]=((int)adc4_val/100)%10+'0'; 
         data_array1[24]=((int)adc4_val/10)%10+'0';  
         data_array1[25]=(int)adc4_val%10+'0';            
         data_array1[26]=',';         
         data_array1[27]=((int)adc5_val/1000)%10+'0';  
         data_array1[28]=((int)adc5_val/100)%10+'0'; 
         data_array1[29]=((int)adc5_val/10)%10+'0';  
         data_array1[30]=(int)adc5_val%10+'0';            
         data_array1[31]=',';         
         data_array1[32]=((int)adc6_val/1000)%10+'0';  
         data_array1[33]=((int)adc6_val/100)%10+'0'; 
         data_array1[34]=((int)adc6_val/10)%10+'0';  
         data_array1[35]=(int)adc6_val%10+'0';            
         data_array1[36]=',';         
         data_array1[37]=((int)adc7_val/1000)%10+'0';  
         data_array1[38]=((int)adc7_val/100)%10+'0'; 
         data_array1[39]=((int)adc7_val/10)%10+'0';  
         data_array1[40]=(int)adc7_val%10+'0';                            
         data_array1[41]='*';
         data_array1[42]='/';

//          do
//        {
            
            err_code = ble_nus_data_send(&m_nus, data_array1, &length1, m_conn_handle);//从机数据上传函数
//            if ((err_code != NRF_ERROR_INVALID_STATE) &&
//               (err_code != NRF_ERROR_RESOURCES) &&
//                (err_code != NRF_ERROR_NOT_FOUND))
//            {
//                APP_ERROR_CHECK(err_code);
//            }
//        } while (err_code == NRF_ERROR_RESOURCES);   
//printf("/*%f,%f,%f,%f,%f ,%f,%f,%f*/",adc0_val* 3.6f/4096.0f,adc1_val* 3.6f/4096.0f,
//         adc2_val* 3.6f/4096.0f,adc3_val* 3.6f/4096.0f,
//         adc4_val* 3.6f/4096.0f,adc5_val* 3.6f/4096.0f,adc6_val* 3.6f/4096.0f,adc7_val* 3.6f/4096.0f);
         temp_val0=0;
         temp_val1=0;
         temp_val2=0;
         temp_val3=0;  
         temp_val4=0;
         temp_val5=0;
         temp_val6=0;
         temp_val7=0; 
         ave_flag=0; 
 
               
     }
    }
}


/**
 * @}
 */
