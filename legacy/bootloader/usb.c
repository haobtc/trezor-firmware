/*
 * This file is part of the Trezor project, https://trezor.io/
 *
 * Copyright (C) 2014 Pavol Rusnak <stick@satoshilabs.com>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <libopencm3/stm32/flash.h>
#include <libopencm3/usb/usbd.h>

#include <string.h>

#include "bootloader.h"
#include "buttons.h"
#include "ecdsa.h"
#include "layout.h"
#include "memory.h"
#include "memzero.h"
#include "oled.h"
#include "rng.h"
#include "secbool.h"
#include "secp256k1.h"
#include "sha2.h"
#include "signatures.h"
#include "si2c.h"
#include "sys.h"
#include "usart.h"
#include "updateble.h"


#include "usb.h"
#include "util.h"

#include "usb21_standard.h"
#include "webusb.h"
#include "winusb.h"

#include "usb_desc.h"
#include "usb_send.h"

enum {
  STATE_READY,
  STATE_OPEN,
  STATE_FLASHSTART,
  STATE_FLASHING,
  STATE_CHECK,
  STATE_END,
};

#define UPDATE_BLE  0x5A
#define UPDATE_ST  0x55


static uint32_t flash_pos = 0, flash_len = 0;
static uint32_t chunk_idx = 0;
static char flash_state = STATE_READY;
static uint8_t update_mode = 0;

static uint32_t FW_HEADER[FLASH_FWHEADER_LEN / sizeof(uint32_t)];
static uint32_t FW_CHUNK[FW_CHUNK_SIZE / sizeof(uint32_t)];
static uint8_t s_ucPackBootRevBuf[64];
static void usb_ble_nfc_poll(void);



static void flash_enter(void) {
  flash_wait_for_last_operation();
  flash_clear_status_flags();
  flash_unlock();
}

static void flash_exit(void) {
  flash_wait_for_last_operation();
  flash_lock();
}

#include "usb_erase.h"

static void check_and_write_chunk(void) {
  uint32_t offset = (chunk_idx == 0) ? FLASH_FWHEADER_LEN : 0;
  uint32_t chunk_pos = flash_pos % FW_CHUNK_SIZE;
  if (chunk_pos == 0) {
    chunk_pos = FW_CHUNK_SIZE;
  }
  uint8_t hash[32] = {0};
  SHA256_CTX ctx = {0};
  sha256_Init(&ctx);
  sha256_Update(&ctx, (const uint8_t *)FW_CHUNK + offset, chunk_pos - offset);
  if (chunk_pos < 64 * 1024) {
    // pad with FF
    for (uint32_t i = chunk_pos; i < 64 * 1024; i += 4) {
      sha256_Update(&ctx, (const uint8_t *)"\xFF\xFF\xFF\xFF", 4);
    }
  }
  sha256_Final(&ctx, hash);

  const image_header *hdr = (const image_header *)FW_HEADER;
  // invalid chunk sent
  if (0 != memcmp(hash, hdr->hashes + chunk_idx * 32, 32)) {
    // erase storage
    erase_storage();
    flash_state = STATE_END;
    vDisp_PromptInfo(DISP_UPDATEDATA_ERROR, true);
    while(false == get_button_response());
    scb_reset_system();

    return;
  }

  flash_enter();
  for (uint32_t i = offset / sizeof(uint32_t); i < chunk_pos / sizeof(uint32_t);
       i++) {
    if(UPDATE_ST == update_mode){
        flash_program_word(
            FLASH_FWHEADER_START + chunk_idx * FW_CHUNK_SIZE + i * sizeof(uint32_t),
            FW_CHUNK[i]);
    }
    else{
        flash_program_word(
            FLASH_BLE_SECTOR9_START + chunk_idx * FW_CHUNK_SIZE + i * sizeof(uint32_t),
            FW_CHUNK[i]);
    }
   
  }
  flash_exit();

  // all done
  if (flash_len == flash_pos) {
    // check remaining chunks if any
    for (uint32_t i = chunk_idx + 1; i < 16; i++) {
      // hash should be empty if the chunk is unused
      if (!mem_is_empty(hdr->hashes + 32 * i, 32)) {
        flash_state = STATE_END;
        vDisp_PromptInfo(DISP_UPDATEDATA_ERROR, true);
        while(false == get_button_response());
        scb_reset_system();
        return;
      }
    }
  }

  memzero(FW_CHUNK, sizeof(FW_CHUNK));
  chunk_idx++;
}

/*
// read protobuf integer and advance pointer
static secbool readprotobufint(const uint8_t **ptr, uint32_t *result) {
  *result = 0;

  for (int i = 0; i <= 3; ++i) {
    *result += (**ptr & 0x7F) << (7 * i);
    if ((**ptr & 0x80) == 0) {
      (*ptr)++;
      return sectrue;
    }
    (*ptr)++;
  }

  if (**ptr & 0xF0) {
    // result does not fit into uint32_t
    *result = 0;

    // skip over the rest of the integer
    while (**ptr & 0x80) (*ptr)++;
    (*ptr)++;
    return secfalse;
  }

  *result += (uint32_t)(**ptr) << 28;
  (*ptr)++;
  return sectrue;
}
*/

static void rx_callback(usbd_device *dev, uint8_t ep) {
  (void)ep;
  static uint16_t msg_id = 0xFFFF;
  static uint8_t buf[64] __attribute__((aligned(4)));
  static uint32_t w;
  static int wi;
  static int old_was_signed;
  
   if(WORK_MODE_USB ==  g_ucWorkMode){
       if (usbd_ep_read_packet(dev, ENDPOINT_ADDRESS_OUT, buf, 64) != 64) return;
   }
   else{
      memcpy(buf,s_ucPackBootRevBuf,64);
   }

  if (flash_state == STATE_END) {
    return;
  }

  if (flash_state == STATE_READY || flash_state == STATE_OPEN ||
      flash_state == STATE_FLASHSTART || flash_state == STATE_CHECK) {
    if (buf[0] != '?' || buf[1] != '#' ||
        buf[2] != '#') {  // invalid start - discard
      return;
    }
    // struct.unpack(">HL") => msg, size
    msg_id = (buf[3] << 8) + buf[4];
  }

  if (flash_state == STATE_READY || flash_state == STATE_OPEN) {
    if (msg_id == 0x0000) {  // Initialize message (id 0)
      send_msg_features(dev);
      flash_state = STATE_OPEN;
      return;
    }
    if (msg_id == 0x0037) {  // GetFeatures message (id 55)
      send_msg_features(dev);
      return;
    }
    if(WORK_MODE_USB !=  g_ucWorkMode){
      if (msg_id == 0x001B) {  // Get button  (id 1B)
        if (button.YesUp) {
          send_msg_success(dev);
        }
        else{
            send_msg_failure_button_request();
            if(button.YesUp ||button.DownUp) {
            BUTTON_CHECK_CLEAR();
            send_msg_failure_button_request();
           }
        }
        return;
      }
    }
    if (msg_id == 0x0001) {  // Ping message (id 1)
      send_msg_success(dev);
      return;
    }
    if (msg_id == 0x0005) {  // WipeDevice message (id 5)
      vDisp_PromptInfo(DISP_FIRMWARE_WIPE_DEVICE, true);
      bool but;

      if(WORK_MODE_USB ==  g_ucWorkMode){
          but = get_button_response();
      }
      else{
         if(button.YesUp ||button.DownUp) {
            BUTTON_CHECK_CLEAR();
            but = button.YesUp;
          }
          else{
             buttonUpdate();
             BUTTON_CHECK_ENBALE();
             send_msg_failure_button_request();
             while(1)
             {
                 usb_ble_nfc_poll();
                 if(button.YesUp ||button.DownUp) {
                    BUTTON_CHECK_CLEAR();
                    break;
                 }
             }
             return;
          }
      }
      if (but) {
        erase_storage_code_progress();
        flash_state = STATE_END;
        send_msg_success(dev);
        vDisp_PromptInfo(DISP_FIRMWARE_WIPE_SUCCESS, true);
        while(false == get_button_response());
        scb_reset_system();
      } else {
        flash_state = STATE_END;
        send_msg_failure(dev);
        vDisp_PromptInfo(DISP_FIRMWARE_WIPE_FAILE, true);
        while(false == get_button_response());
        scb_reset_system();
      }
      return;
    }
  }

  if (flash_state == STATE_OPEN) {
    if (msg_id == 0x0006) {  // FirmwareErase message (id 6)
      bool proceed = false;
      if (firmware_present_new()) {
        vDisp_PromptInfo(DISP_UPDATE_USB, true);
        if(WORK_MODE_USB ==  g_ucWorkMode){
            proceed = get_button_response();
        }
        else{
           if(button.YesUp ||button.DownUp) {
              BUTTON_CHECK_CLEAR();
              proceed = button.YesUp;
            }
            else{
               buttonUpdate();
               BUTTON_CHECK_ENBALE();
               send_msg_failure_button_request();
               while(1)
               {
                   usb_ble_nfc_poll();
                   if(button.YesUp ||button.DownUp) {
                      BUTTON_CHECK_CLEAR();
                      break;
                   }
               }
               return;
            }
        }

      } else {
        proceed = true;
      }
      if (proceed) {
        // check whether the current firmware is signed (old or new method)
        if (firmware_present_new()) {
          const image_header *hdr =
              (const image_header *)FLASH_PTR(FLASH_FWHEADER_START);
          old_was_signed =
              signatures_new_ok(hdr, NULL) & check_firmware_hashes(hdr);
        } else if (firmware_present_old()) {
          old_was_signed = signatures_old_ok();
        } else {
          old_was_signed = SIG_FAIL;
        }
        erase_code_progress();
        erase_ble_code_progress();
        send_msg_success(dev);
        flash_state = STATE_FLASHSTART;
      } else {
            send_msg_failure(dev);
            flash_state = STATE_END;
            vDisp_PromptInfo(DISP_UPDATEDATA_CANCEL, true);
            while(false == get_button_response());
            scb_reset_system();
      }
      return;
    }
    if (msg_id == 0x0002) {  // FirmwareErase message (id 2)
      bool proceed = false;
      vDisp_PromptInfo(DISP_UPDATE_BLE  , true);
      if(WORK_MODE_USB ==  g_ucWorkMode){
          proceed = get_button_response();
      }
      else{
         if(button.YesUp ||button.DownUp) {
            BUTTON_CHECK_CLEAR();
            proceed = button.YesUp;
          }
          else{
             buttonUpdate();
             BUTTON_CHECK_ENBALE();
             send_msg_failure_button_request();
             while(1)
             {
                 usb_ble_nfc_poll();
                 if(button.YesUp ||button.DownUp) {
                    BUTTON_CHECK_CLEAR();
                    break;
                 }
             }
             return;
          }
      }
      if (proceed) {
        erase_ble_code_progress();
        send_msg_success(dev);
        flash_state = STATE_FLASHSTART;
      } else {
        send_msg_failure(dev);
        flash_state = STATE_END;
        vDisp_PromptInfo(DISP_UPDATEDATA_CANCEL, true);
        while(false == get_button_response());
        scb_reset_system();
      }
      return;
    }
    return;
  }

  if (flash_state == STATE_FLASHSTART) {
    if (msg_id == 0x0007) {  // FirmwareUpload message (id 7)
      if (buf[9] != 0x0a) {  // invalid contents
        send_msg_failure(dev);
        flash_state = STATE_END;
        vDisp_PromptInfo(DISP_UPDATEDATA_ERROR, true);
        while(false == get_button_response());
        scb_reset_system();
        return;
      }
      // read payload length
      /*
      const uint8_t *p = buf + 10;
      if (readprotobufint(&p, &flash_len) != sectrue) {  // integer too large
        send_msg_failure(dev);
        flash_state = STATE_END;
        show_halt("Firmware is", "too big.");
        return;
      }
      */
      const uint8_t *p = buf + 13;
      flash_len = (buf[5] <<24) + (buf[6] <<16) + (buf[7] <<8) +(buf[8]) - 4;
    
      // check firmware magic
      if ((memcmp(p, &FIRMWARE_MAGIC_NEW, 4) != 0)  && (memcmp(p, &FIRMWARE_MAGIC_BLE, 4) != 0)){
        send_msg_failure(dev);
        flash_state = STATE_END;
        vDisp_PromptInfo(DISP_UPDATEDATA_ERROR, true);
        while(false == get_button_response());
        scb_reset_system();
        return;
      }
      if(memcmp(p, &FIRMWARE_MAGIC_NEW, 4)  ==  0){
        update_mode = UPDATE_ST;
      }
      else{
        update_mode = UPDATE_BLE;
      }
      if (flash_len <= FLASH_FWHEADER_LEN) {  // firmware is too small
        send_msg_failure(dev);
        flash_state = STATE_END;
        vDisp_PromptInfo(DISP_UPDATEDATA_ERROR, true);
        while(false == get_button_response());
        scb_reset_system();
        return;
      }
      if (UPDATE_ST == update_mode){
          if (flash_len >
              FLASH_FWHEADER_LEN + FLASH_APP_LEN) {  // firmware is too big
            send_msg_failure(dev);
            flash_state = STATE_END;
            vDisp_PromptInfo(DISP_UPDATEDATA_ERROR, true);
            while(false == get_button_response());
            scb_reset_system();
            return;
          }
      }
      else{
          if (flash_len >
              FLASH_FWHEADER_LEN + FLASH_BLE_MAX_LEN) {  // firmware is too big
            send_msg_failure(dev);
            flash_state = STATE_END;
            vDisp_PromptInfo(DISP_UPDATEDATA_ERROR, true);
            while(false == get_button_response());
            scb_reset_system();
            return;
          }
      }
      memzero(FW_HEADER, sizeof(FW_HEADER));
      memzero(FW_CHUNK, sizeof(FW_CHUNK));
      flash_state = STATE_FLASHING;
      flash_pos = 0;
      chunk_idx = 0;
      w = 0;
      while (p < buf + 64) {
        // assign byte to first byte of uint32_t w
        w = (w >> 8) | (((uint32_t)*p) << 24);
        wi++;
        if (wi == 4) {
          FW_HEADER[flash_pos / 4] = w;
          flash_pos += 4;
          wi = 0;
        }
        p++;
      }
      return;
    }
    return;
  }

  if (flash_state == STATE_FLASHING) {
    if (buf[0] != '?') {  // invalid contents
      send_msg_failure(dev);
      flash_state = STATE_END;
      vDisp_PromptInfo(DISP_UPDATEDATA_ERROR, true);
      while(false == get_button_response());
      scb_reset_system();
      return;
    }

    static uint8_t flash_anim = 0;
    if (flash_anim % 32 == 4) {
      layoutUpdate(1000 * flash_pos / flash_len);
    }
    flash_anim++;

    const uint8_t *p = buf + 1;
    while (p < buf + 64 && flash_pos < flash_len) {
      // assign byte to first byte of uint32_t w
      w = (w >> 8) | (((uint32_t)*p) << 24);
      wi++;
      if (wi == 4) {
        if (flash_pos < FLASH_FWHEADER_LEN) {
          FW_HEADER[flash_pos / 4] = w;
        } else {
          FW_CHUNK[(flash_pos % FW_CHUNK_SIZE) / 4] = w;
        }
        flash_pos += 4;
        wi = 0;
        // finished the whole chunk
        if (flash_pos % FW_CHUNK_SIZE == 0) {
          check_and_write_chunk();
        }
      }
      p++;
    }
    // flashing done
    if (flash_pos == flash_len) {
      // flush remaining data in the last chunk
      if (flash_pos % FW_CHUNK_SIZE > 0) {
        check_and_write_chunk();
      }
      flash_state = STATE_CHECK;
      const image_header *hdr = (const image_header *)FW_HEADER;
      if (SIG_OK != signatures_new_ok(hdr, NULL)) {
        send_msg_buttonrequest_firmwarecheck(dev);
        return;
      }
    } else {
      return;
    }
  }

  if (flash_state == STATE_CHECK) {
    // use the firmware header from RAM
    const image_header *hdr = (const image_header *)FW_HEADER;

    bool hash_check_ok;
    // show fingerprint of unsigned firmware
    if (SIG_OK != signatures_new_ok(hdr, NULL)) {
      if (msg_id != 0x001B) {  // ButtonAck message (id 27)
        return;
      }
      uint8_t hash[32] = {0};
      compute_firmware_fingerprint(hdr, hash);
      //layoutFirmwareFingerprint(hash);
      //hash_check_ok = get_button_response();
      hash_check_ok = false;
    } else {
      hash_check_ok = true;
    }

    if(UPDATE_ST == update_mode){
        layoutUpdate(1000);
        // wipe storage if:
        // 1) old firmware was unsigned or not present
        // 2) signatures are not OK
        // 3) hashes are not OK
        if (SIG_OK != old_was_signed || SIG_OK != signatures_new_ok(hdr, NULL) ||
            SIG_OK != check_firmware_hashes(hdr)) {
          // erase storage
          erase_storage();
          // check erasure
          uint8_t hash[32] = {0};
          sha256_Raw(FLASH_PTR(FLASH_STORAGE_START), FLASH_STORAGE_LEN, hash);
          if (memcmp(hash,
                     "\x2d\x86\x4c\x0b\x78\x9a\x43\x21\x4e\xee\x85\x24\xd3\x18\x20"
                     "\x75\x12\x5e\x5c\xa2\xcd\x52\x7f\x35\x82\xec\x87\xff\xd9\x40"
                     "\x76\xbc",
                     32) != 0) {
            send_msg_failure(dev);
            vDisp_PromptInfo(DISP_UPDATE_FAIL, true);
            while(false == get_button_response());
            scb_reset_system();
            return;
          }
        }

        flash_enter();
        // write firmware header only when hash was confirmed
        if (hash_check_ok) {
          for (size_t i = 0; i < FLASH_FWHEADER_LEN / sizeof(uint32_t); i++) {
            flash_program_word(FLASH_FWHEADER_START + i * sizeof(uint32_t),
                               FW_HEADER[i]);
          }
        } else {
          for (size_t i = 0; i < FLASH_FWHEADER_LEN / sizeof(uint32_t); i++) {
            flash_program_word(FLASH_FWHEADER_START + i * sizeof(uint32_t), 0);
          }
        }
        flash_exit();

        flash_state = STATE_END;
        if (hash_check_ok) {
          vDisp_PromptInfo(DISP_UPDATGE_SUCCESS, true);
          send_msg_success(dev);
          while(false == get_button_response());
          scb_reset_system();
        } else {
          vDisp_PromptInfo(DISP_UPDATGE_SUCCESS, true);
          send_msg_failure(dev);
          while(false == get_button_response());
          scb_reset_system();
        }
        return;
   }
   else{
        layoutUpdate(50000);
        
        flash_enter();
        // write firmware header only when hash was confirmed
        for (size_t i = 0; i < FLASH_FWHEADER_LEN / sizeof(uint32_t); i++) {
          flash_program_word(FLASH_BLE_SECTOR9_START + i * sizeof(uint32_t),
                             FW_HEADER[i]);
        }
        flash_exit();

        flash_state = STATE_END;
        send_msg_success(dev);

        unsigned int uiBleLen = flash_len - FLASH_FWHEADER_LEN;
  	    if (FALSE == bUBLE_UpdateBleFirmware(uiBleLen,FLASH_BLE_SECTOR9_START+FLASH_FWHEADER_LEN,ERASE_ALL))
		{
		    send_msg_failure(dev);
		    vDisp_PromptInfo(DISP_UPDATE_FAIL, true);
            while(false == get_button_response());
            scb_reset_system();
			return;		
		}
		//ble power rest 
		POWER_OFF_BLE();
	    delay_time(10);
	    POWER_ON_BLE();
        vDisp_PromptInfo(DISP_UPDATGE_SUCCESS, true);
        while(false == get_button_response());
        scb_reset_system();
        return;
   
   }
  }
}

static void set_config(usbd_device *dev, uint16_t wValue) {
  (void)wValue;

  usbd_ep_setup(dev, ENDPOINT_ADDRESS_IN, USB_ENDPOINT_ATTR_INTERRUPT, 64, 0);
  usbd_ep_setup(dev, ENDPOINT_ADDRESS_OUT, USB_ENDPOINT_ATTR_INTERRUPT, 64,
                rx_callback);
}

static usbd_device *usbd_dev;
static uint8_t usbd_control_buffer[256] __attribute__((aligned(2)));

static const struct usb_device_capability_descriptor *capabilities[] = {
    (const struct usb_device_capability_descriptor
         *)&webusb_platform_capability_descriptor,
};

static const struct usb_bos_descriptor bos_descriptor = {
    .bLength = USB_DT_BOS_SIZE,
    .bDescriptorType = USB_DT_BOS,
    .bNumDeviceCaps = sizeof(capabilities) / sizeof(capabilities[0]),
    .capabilities = capabilities};

static void usbInit(void) {
   if(WORK_MODE_USB ==  g_ucWorkMode)
   {
        usbd_dev = usbd_init(&otgfs_usb_driver, &dev_descr, &config, usb_strings,
                       sizeof(usb_strings) / sizeof(const char *),
                       usbd_control_buffer, sizeof(usbd_control_buffer));
        usbd_register_set_config_callback(usbd_dev, set_config);
        usb21_setup(usbd_dev, &bos_descriptor);
        webusb_setup(usbd_dev, "trezor.io/start");
        winusb_setup(usbd_dev, USB_INTERFACE_INDEX_MAIN);
   }
   else
   {
        vSI2CDRV_Init();
        POWER_ON_BLE();
   }
}
void bootloader_logo(void) 
{
    oledClear();
    oledDrawBitmap(0, 0, &bmp_logo64);
    if (firmware_present_new()) {
      oledDrawStringCenter(90, 10, "Bixin", FONT_STANDARD);
      oledDrawStringCenter(90, 30, "Bootloader", FONT_STANDARD);
      oledDrawStringCenter(90, 50,
                           VERSTR(VERSION_MAJOR) "." VERSTR(
                               VERSION_MINOR) "." VERSTR(VERSION_PATCH),
                           FONT_STANDARD);
    } else {
      oledDrawStringCenter(90, 10, "Welcome!", FONT_STANDARD);
      oledDrawStringCenter(90, 30, "Please visit", FONT_STANDARD);
      oledDrawStringCenter(90, 50, "bixin.com", FONT_STANDARD);
    }
    oledRefresh();
}

static void vBle_NFC_RX_Data(uint8_t *pucInputBuf)
{
    uint16_t i,usLen;
    uint8_t ucCmd;
    
    ucCmd = pucInputBuf[0];
    usLen =(pucInputBuf[1]<<8) + (pucInputBuf[2]&0xFF) - CRC_LEN;
    #if(_SUPPORT_DEBUG_UART_)
    //vUART_DebugInfo("\n\r vBle_NFC_RX_Data !\n\r",pucInputBuf,usLen+3);
    #endif
    switch(ucCmd)
    {
        case APDU_TAG_BLE:
            if(true == bBle_DisPlay(pucInputBuf[DATA_HEAD_LEN],pucInputBuf+DATA_HEAD_LEN+1))
            {
                 bootloader_logo();
            }
        break;
        case APDU_TAG_BLE_NFC:
            if(0x3F == pucInputBuf[DATA_HEAD_LEN])
            { 
                for(i=0;i<usLen/64;i++)
                { 
		            memcpy(s_ucPackBootRevBuf,pucInputBuf+DATA_HEAD_LEN+i*64,64);
                    rx_callback(NULL,0);
                }
                if(usLen%64)
                {
                    memcpy(s_ucPackBootRevBuf,pucInputBuf+DATA_HEAD_LEN+i*64,usLen%64);
                    rx_callback(NULL,0);
                }
                if(usLen >=0x400)
                {
                    send_msg_success(NULL);
                }

            }
            else
            {
                s_ucPackBootRevBuf[0] = 0x60;
                s_ucPackBootRevBuf[1] = 0x00;
                vSI2CDRV_SendResponse(s_ucPackBootRevBuf,2);
            }
                
        break;
        case APDU_TAG_BAT:
            g_ucBatValue =  pucInputBuf[DATA_HEAD_LEN];
            s_ucPackBootRevBuf[0] = 0x90;
            s_ucPackBootRevBuf[1] = 0x00;
            vSI2CDRV_SendResponse(s_ucPackBootRevBuf,2);
        break;
        case APDU_TAG_HANDSHAKE:
         memcpy(g_ble_info.ucBle_Mac,pucInputBuf+DATA_HEAD_LEN,BLE_MAC_LEN);
         memcpy(g_ble_info.ucBle_Version,pucInputBuf+DATA_HEAD_LEN+BLE_MAC_LEN,2);
         g_ucBatValue = pucInputBuf[DATA_HEAD_LEN+BLE_MAC_LEN+2];
         memset(g_ble_info.ucBle_Name,0x00,sizeof(g_ble_info.ucBle_Name));
         vCalu_BleName(g_ble_info.ucBle_Mac,g_ble_info.ucBle_Name);
         memcpy(s_ucPackBootRevBuf,g_ble_info.ucBle_Name,BLE_ADV_NAME_LEN);
	     vSI2CDRV_SendResponse(s_ucPackBootRevBuf,BLE_ADV_NAME_LEN);
        break;
        default:
        break;
    }
   
    
}

static void usb_ble_nfc_poll(void)
{
    if(WORK_MODE_USB ==  g_ucWorkMode)
    {
	    usbd_poll(usbd_dev);
    }
    else
    {  
        memset(g_ucI2cRevBuf,0x00,sizeof(g_ucI2cRevBuf));
	    if(true == bSI2CDRV_ReceiveData(g_ucI2cRevBuf))
	    {
	        vBle_NFC_RX_Data(g_ucI2cRevBuf);
	    }
    }
    
}


static void checkButtons(void) {
  static bool btn_left = false, btn_right = false, btn_final = false;
  if (btn_final) {
    return;
  }
  uint16_t state = gpio_port_read(BTN_PORT);
  if ((state & (BTN_PIN_YES | BTN_PIN_DOWN)) != (BTN_PIN_YES | BTN_PIN_DOWN)) {
    if ((state & BTN_PIN_DOWN) != BTN_PIN_DOWN) {
      btn_left = true;
    }
    if ((state & BTN_PIN_YES) != BTN_PIN_YES) {
      btn_right = true;
    }
  }
  if (btn_left) {
    oledBox(0, 0, 3, 3, true);
  }
  if (btn_right) {
    oledBox(OLED_WIDTH - 4, 0, OLED_WIDTH - 1, 3, true);
  }
  if (btn_left || btn_right) {
    oledRefresh();
  }
  if (btn_left && btn_right) {
    btn_final = true;
  }
}

void usbLoop(void) {
  bool firmware_present = firmware_present_new();
  usbInit();
  for (;;) {
    usb_ble_nfc_poll();
    if (!firmware_present &&
        (flash_state == STATE_READY || flash_state == STATE_OPEN)) {
      checkButtons();
    }
  }
}
