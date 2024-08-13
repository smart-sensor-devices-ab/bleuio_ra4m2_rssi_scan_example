/***********************************************************************************************************************
 * File Name    : usb_hcdc_app.c
 * Description  : Contains data structures and functions used in usb_hcdc_app.c.
 **********************************************************************************************************************/
/***********************************************************************************************************************
 * DISCLAIMER
 * This software is supplied by Renesas Electronics Corporation and is only intended for use with Renesas products. No
 * other uses are authorized. This software is owned by Renesas Electronics Corporation and is protected under all
 * applicable laws, including copyright laws.
 * THIS SOFTWARE IS PROVIDED "AS IS" AND RENESAS MAKES NO WARRANTIES REGARDING
 * THIS SOFTWARE, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. ALL SUCH WARRANTIES ARE EXPRESSLY DISCLAIMED. TO THE MAXIMUM
 * EXTENT PERMITTED NOT PROHIBITED BY LAW, NEITHER RENESAS ELECTRONICS CORPORATION NOR ANY OF ITS AFFILIATED COMPANIES
 * SHALL BE LIABLE FOR ANY DIRECT, INDIRECT, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES FOR ANY REASON RELATED TO THIS
 * SOFTWARE, EVEN IF RENESAS OR ITS AFFILIATES HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 * Renesas reserves the right, without notice, to make changes to this software and to discontinue the availability of
 * this software. By using this software, you agree to the additional terms and conditions found by accessing the
 * following link:
 * http://www.renesas.com/disclaimer
 *
 * Copyright (C) 2020 Renesas Electronics Corporation. All rights reserved.
 ***********************************************************************************************************************/

#include <usb_hcdc_app.h>
#include <r_usb_cstd_rtos.h>
#include <common_utils.h>
#include <usb_thread.h>
#include "r_usb_basic.h"
#include "r_usb_hcdc.h"

/*******************************************************************************************************************//**
 * @addtogroup usb_hcdc_ep
 * @{
 **********************************************************************************************************************/
/** Private variables **/
static usb_hcdc_linecoding_t g_serial_state;
static usb_hcdc_linecoding_t g_com_parm;
static usb_hcdc_device_info_t dev_info;
static uint8_t  g_rcv_buf[CDC_READ_DATA_LEN];               //Receive buffer
static uint8_t g_snd_buf[CDC_WRITE_DATA_LEN];               //Send buffer
static uint8_t  g_usb_dummy = RESET_VALUE;                  //dummy variable to send
static volatile bool g_err_flag = false;                    //flag bit

/** Private functions**/
static void set_line_coding (usb_instance_ctrl_t * p_ctrl, uint8_t device_address);
static void set_control_line_state (usb_instance_ctrl_t * p_ctrl, uint8_t device_address);
static void get_line_coding (usb_instance_ctrl_t * p_ctrl, uint8_t device_address);
static void usb_data_process(usb_event_info_t *event_info);
static void handle_error(fsp_err_t err, char *err_str);

#define BLEUIO_CMD_CENTRAL "AT+CENTRAL\r"
#define BLEUIO_CMD_SHOW_RSSI "AT+SHOWRSSI=1\r"
/*Change ADDR_TO_SCAN to a nearby device to be scanned*/
#define ADDR_TO_SCAN "[0]D0:76:50:80:01:75"
#define BLEUIO_CMD_SCANTARGET ("AT+SCANTARGET="ADDR_TO_SCAN"\r")

bool central_set = false;
bool show_rssi_set = false;
bool scan_started = false;

static char formatted_msg[20];

/******************************************************************************************************//**
* @brief This function process the data from the host to device and prints the received data on RTT terminal
* @param[IN]   None
* @retval      None
*******************************************************************************************************/
void usb_hcdc_task(void)
{
    fsp_err_t err = FSP_SUCCESS;
    static usb_event_info_t *event_info = NULL;
    BaseType_t err_queue = pdFALSE;
    memset(&g_serial_state, RESET_VALUE, sizeof(g_serial_state));
    memset(&g_com_parm, RESET_VALUE, sizeof(g_com_parm));
    /*Prepare first BleuIO AT command*/
    sprintf((char *)g_snd_buf, "%s", (char *)BLEUIO_CMD_CENTRAL);

    /* Example Project information printed on the Console */
    APP_PRINT(BANNER_INFO);
    APP_PRINT(EP_INFO);


    err = R_USB_Open (&g_basic_ctrl, &g_basic_cfg);
    /* Error Handle */
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT ("\r\nError in initializing USB Host CDC\r\n");
        APP_ERR_TRAP (err);
    }


    APP_PRINT("\r\nStarting Example.\r\nPlease wait for the dongle to exit booloader mode...");
    while (true)
    {

        /* Handle error if queue send fails*/
        if (true == g_err_flag)
        {
            handle_error(g_err_flag, "Error in sending usb event through queue");
        }

        /* Receive message from queue and analyzing the received message*/
         err_queue= xQueueReceive(g_usb_queue, &event_info,(portMAX_DELAY));
         /* Handle error */
         if(pdTRUE != err_queue)
         {
             handle_error (err_queue, "Error in receiving USB event message through queue");
         }

        switch (event_info->event)
        {
            case USB_STATUS_CONFIGURED:
            {
                /* CDC Class request "SetLineCoding" */
                set_line_coding (&g_basic_ctrl, event_info->device_address);
                APP_PRINT("\r\nUSB_STATUS_CONFIGURED");
            }
            break;

            case USB_STATUS_READ_COMPLETE:
            {
                /* CDC class communication data process */
                usb_data_process(event_info);
            }
            break;

            case USB_STATUS_WRITE_COMPLETE:
            {
                APP_PRINT("\r\nUSB_STATUS_WRITE_COMPLETE");
            }
            break;

            case USB_STATUS_REQUEST_COMPLETE:
            {
                /* Check Complete request "SetLineCoding" */
                if (USB_CDC_SET_LINE_CODING == (event_info->setup.request_type & USB_BREQUEST))
                {
                    APP_PRINT("\r\nUSB_CDC_SET_LINE_CODING REQ");
                    /* Class notification "SerialState" receive start */
                    set_control_line_state (&g_basic_ctrl, event_info->device_address);
                }
                /* Check Complete request "SetControlLineState" */
                else if (USB_CDC_SET_CONTROL_LINE_STATE == (event_info->setup.request_type & USB_BREQUEST))
                {
                    APP_PRINT("\r\nUSB_CDC_SET_CONTROL_LINE_STATE REQ");
                    /* CDC Class request "SetLineCoding" */
                    get_line_coding (&g_basic_ctrl, event_info->device_address);
                }
                else if (USB_CDC_GET_LINE_CODING == (event_info->setup.request_type & USB_BREQUEST))
                {
                    /*Read device info*/
                    err = R_USB_HCDC_DeviceInfoGet(&g_basic_ctrl, &dev_info, event_info->device_address);
                    APP_PRINT("\r\nUSB_CDC_GET_LINE_CODING REQ");
                    if (FSP_SUCCESS != err)
                    {
                        handle_error (err, "**R_USB_HCDC_DeviceInfoGet API FAILED**");
                    }
                    APP_PRINT("\r\n\r\n PID %04X", dev_info.product_id);
                    APP_PRINT("\r\n SUBCLASS %02X", dev_info.subclass);
                    APP_PRINT("\r\n VID %04X\r\n", dev_info.vendor_id);

                    /*Check that BleuIO is out of bootloader mode*/
                    if((dev_info.product_id == 0x6002) && (dev_info.vendor_id == 0x2DCF))
                    {
                        /* Report receive start */
                        R_USB_Read (&g_basic_ctrl, g_rcv_buf, CDC_READ_DATA_LEN,
                                              event_info->device_address);
                        /* Handle Error */
                        if (FSP_SUCCESS != err)
                        {
                            handle_error (err,"**R_USB_Read API FAILED**");
                        }
                        err = R_USB_Write (&g_basic_ctrl, g_snd_buf, strlen((char *)BLEUIO_CMD_CENTRAL),
                                     event_info->device_address);
                        if (FSP_SUCCESS != err)
                        {
                            handle_error (err, "**R_USB_Write API FAILED**");
                        }
                    }
                }
                else
                {
                    /* Not support request */
                }
            }
            break;

            default:
            {
                APP_PRINT("\r\nUnknown operation: %02X", event_info->event);
                /* No operation to do*/
            }
            break;
        }

    }
}
 /* End of function main_task() */

/*******************************************************************************************************************//**
 * @brief In this function initializes to set control line state information for host control transfer.
 * @param[IN]   p_ctrl         Pointer to control structure
 * @param[IN]   device_address         USB device address
 * @retval      None.
 ***********************************************************************************************************************/
static void set_control_line_state (usb_instance_ctrl_t * p_ctrl, uint8_t device_address)
{
    usb_setup_t setup;
    fsp_err_t err = FSP_SUCCESS;


    setup.request_type      = SET_CONTROL_LINE_STATE;   /* bRequestCode:SET_CONTROL_LINE_STATE, bmRequestType */
    setup.request_value     = 3U;                           /* wValue:3U (Enables DTR and RTS)*/
    setup.request_index     = VALUE_ZERO;                   /* wIndex:Interface */
    setup.request_length    = VALUE_ZERO;                   /* wLength:Zero */

    err = R_USB_HostControlTransfer(p_ctrl, &setup, &g_usb_dummy, device_address);
    if (FSP_SUCCESS != err)
    {
        handle_error (err, "**R_USB_HOSTControlTransfer API FAILED**");
    }

} /* End of function cdc_set_control_line_state */
/*******************************************************************************************************************//**
 * @brief In this function initializes to set line coding information for host control transfer.
 * @param[IN]   p_ctrl         Pointer to control structure
 * @param[IN]   device_address  USB device address
 * @retval      None.
 ***********************************************************************************************************************/
 static void set_line_coding (usb_instance_ctrl_t * p_ctrl, uint8_t device_address)
{
     usb_setup_t setup;
     fsp_err_t err = FSP_SUCCESS;
     g_com_parm.dwdte_rate   = (uint32_t)USB_HCDC_SPEED_9600;
     g_com_parm.bdata_bits   = USB_HCDC_DATA_BIT_8;
     g_com_parm.bchar_format = USB_HCDC_STOP_BIT_1;
     g_com_parm.bparity_type = USB_HCDC_PARITY_BIT_NONE;


     setup.request_type      = SET_LINE_CODING;    /* bRequestCode:SET_LINE_CODING, bmRequestType */
     setup.request_value     = VALUE_ZERO;             /* wValue:Zero */
     setup.request_index     = VALUE_ZERO;             /* wIndex:Interface */
     setup.request_length    = LINE_CODING_LENGTH; /* Data:Line Coding Structure */

     /* Request Control transfer */
     err = R_USB_HostControlTransfer(p_ctrl, &setup, (uint8_t *)&g_com_parm, device_address);
     if (FSP_SUCCESS != err)
     {
         handle_error (err, "**R_USB_HostControlTransfer API FAILED**");
     }
} /* End of function cdc_set_line_coding */

 /*******************************************************************************************************************//**
  * @brief In this function initializes to get line coding information for host control transfer.
  * @param[IN]   p_ctrl                pointer to control structure
  * @param[IN]   device_address         USB device address
  * @retval      None.
  ***********************************************************************************************************************/
static void get_line_coding (usb_instance_ctrl_t * p_ctrl, uint8_t device_address)
{
    usb_setup_t setup;
    fsp_err_t err = FSP_SUCCESS;
    setup.request_type      = GET_LINE_CODING;      /* bRequestCode:GET_LINE_CODING, bmRequestType */
    setup.request_value     = VALUE_ZERO;               /* wValue:Zero */
    setup.request_index     = VALUE_ZERO;               /* wIndex:Interface */
    setup.request_length    = LINE_CODING_LENGTH;   /* Data:Line Coding Structure */

    /* Request Control transfer */
    err = R_USB_HostControlTransfer(p_ctrl, &setup, (uint8_t *)&g_com_parm, device_address);
    if (FSP_SUCCESS != err)
    {
        handle_error (err, "**R_USB_HostControlTransfer API FAILED**");
    }

} /* End of function cdc_get_line_coding */

/*******************************************************************************************************************//**
 * @brief  This function is called to do closing of usb module using its HAL level API and handles the error trap.
 *  Handle the Error internally with Proper Message. Application handles the rest.
 * @param[IN] err       Return values of APIs
 * @param[IN] err_str   Print message from the failed API call
 * @retval    None
 **********************************************************************************************************************/
static void handle_error(fsp_err_t err, char *err_str)
{
    fsp_err_t error = FSP_SUCCESS;

    /* close opened USB module */
    error = R_USB_Close(&g_basic_ctrl);
    /* Handle error */
    if(FSP_SUCCESS != error)
    {
        APP_ERR_PRINT ("**\r\n R_USB_Close API FAILED **\r\n");
    }
    APP_ERR_PRINT(err_str);
    APP_ERR_TRAP(err);
} /* End of function handle_error() */

/*******************************************************************************************************************//**
 * @brief     This function is callback for FreeRTOS+HCDC and triggers when USB event occurs from the device.
 * @param[IN]   p_event_info    Triggered event
 * @param[IN]   cur_task        current task handle
 * @param[IN]   usb_state       State of USB
 * @retval      None.
 ***********************************************************************************************************************/
void usb_rtos_callback (usb_event_info_t *p_event_info, usb_hdl_t cur_task, usb_onoff_t usb_state)
{
    FSP_PARAMETER_NOT_USED (cur_task);
    FSP_PARAMETER_NOT_USED (usb_state);

    /* Send event received to queue */
    if (pdTRUE != (xQueueSend(g_usb_queue, (const void *)&p_event_info, (TickType_t)(NO_WAIT_TIME))))
    {
        g_err_flag = true;
    }
} /* End of function usb_rtos_callback */
/*******************************************************************************************************************//**
  * @brief This function is to do data process with peripheral device
  * @param[IN]   event_info             data process in HCDC event type
  * @retval      None.
  ***********************************************************************************************************************/
void usb_data_process(usb_event_info_t *event_info)
{
    fsp_err_t err = FSP_SUCCESS;
    char * OK_STR = "OK";
    char * SCANNING_TARGET_STR = "SCANNING TARGET DEVICE...";
    size_t msg_len = 0;

    if (USB_CLASS_HCDC == event_info->type)
    {
        if (RESET_VALUE < event_info->data_size)
        {

            if(scan_started && (event_info->data_size > 9))
            {
                memcpy(formatted_msg, g_rcv_buf +2, 9);
                APP_PRINT("\r\n>> %s ", formatted_msg);
            }
            else
            {
                APP_PRINT(">%s\r\n", g_rcv_buf);
            }

            /*Check if we received 'OK' response or if we received a 'SCANNING TARGET DEVICE...' response */
            if(strstr((char *)&g_rcv_buf, OK_STR) != NULL)
            {
                /*Clear send buffer*/
                memset(g_snd_buf, 0, sizeof(g_snd_buf));

                /*Check what command to send next*/
                if(!central_set)
                {
                    /*Set flag*/
                    central_set = true;
                    /*Prepare command and set command length */
                    sprintf((char *)g_snd_buf, "%s", (char *)BLEUIO_CMD_SHOW_RSSI);
                    msg_len = strlen((char *)g_snd_buf);
                } else if (!show_rssi_set)
                {
                    /*Set flag*/
                    show_rssi_set = true;
                    /*Prepare command and set command length */
                    sprintf((char *)g_snd_buf, "%s", (char *)BLEUIO_CMD_SCANTARGET);
                    msg_len = strlen((char *)g_snd_buf);
                }
                err = R_USB_Write (&g_basic_ctrl, g_snd_buf, msg_len,
                             event_info->device_address);
                if (FSP_SUCCESS != err)
                {
                    handle_error (err, "**R_USB_Write API FAILED**");
                }
            }
            else if(strstr((char *)&g_rcv_buf, SCANNING_TARGET_STR) != NULL)
            {
                /*Set flag*/
                scan_started = true;
            }

            /*Clear receive buffer*/
            memset(g_rcv_buf, 0, sizeof(g_rcv_buf));
            /* Report receive start */
            R_USB_Read (&g_basic_ctrl, g_rcv_buf, CDC_READ_DATA_LEN,
                                  event_info->device_address);
            /* Handle Error */
            if (FSP_SUCCESS != err)
            {
                handle_error (err,"**R_USB_Read API FAILED**");
            }
        }
        else
        {
            /* Send the data reception request when the zero-length packet is received. */
            err = R_USB_Read (&g_basic_ctrl,g_rcv_buf, event_info->data_size,
                        event_info->device_address);
            if (FSP_SUCCESS != err)
            {
                handle_error (err, "**R_USB_Read API FAILED**");
            }
        }
    }
    else
    {
        /* Class notification "SerialState" receive start */
        err = R_USB_Read (&g_basic_ctrl, (uint8_t *) &g_serial_state,
        USB_HCDC_SERIAL_STATE_MSG_LEN,event_info->device_address);
        /* Error Handle */
        if (FSP_SUCCESS != err)
        {
            handle_error (err, "**R_USB_Read API FAILED**");
        }
    }
} /* End of function usb_data_process */


/*******************************************************************************************************************//**
 * @} (end addtogroup usb_hcdc_ep)
 **********************************************************************************************************************/
