/***********************************************************************************************************************
 * File Name    : usb_hcdc_app.h
 * Description  : Contains data structures and functions used in usb_hcdc_app.h.
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


#ifndef USB_HCDC_APP_H_
#define USB_HCDC_APP_H_
/** Macros definitions **/

#define EP_INFO        "\r\nThis example project demonstrates how to use the USB Host Communication Device \r\n"\
                       "Class (HCDC) driver on Renesas RA MCUs to communicate with a BleuIO dongle.\r\n"\
                       "When the BlueIO has been identified, the board will send commands to make it scan.\r\n"\
                       "The RSSI value will be parsed from the advertising and scan response data.\r\n"\
                       "The board will print the RSSI value on the RTTViewer.\r\n\n\n"

#define SET_LINE_CODING             (USB_CDC_SET_LINE_CODING | USB_HOST_TO_DEV | USB_CLASS | USB_INTERFACE)
#define GET_LINE_CODING             (USB_CDC_GET_LINE_CODING | USB_DEV_TO_HOST | USB_CLASS | USB_INTERFACE)
#define SET_CONTROL_LINE_STATE      (USB_CDC_SET_CONTROL_LINE_STATE | USB_HOST_TO_DEV | USB_CLASS | USB_INTERFACE)
#define LINE_CODING_LENGTH          (0x07U)
#define CONTROL_LINE_STATE_LENGTH   (0x02U)
#define VALUE_ZERO                  (0x0000U)
#define NO_WAIT_TIME                0
#define CDC_READ_DATA_LEN           512
#define CDC_WRITE_DATA_LEN          512
#define ZERO_INDEX                  0

/** Function declarations **/
void usb_hcdc_task(void);


#endif /* USB_HCDC_APP_H_ */
