/*
 * This file is part of the Trezor project, https://trezor.io/
 *
 * Copyright (C) 2018 Pavol Rusnak <stick@satoshilabs.com>
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
#include "mi2c.h"
#include "se_chip.h"
#include "storage.h"

bool get_features(Features *resp) {
  resp->has_vendor = true;
  strlcpy(resp->vendor, "trezor.io", sizeof(resp->vendor));
  resp->has_major_version = true;
  resp->major_version = VERSION_MAJOR;
  resp->has_minor_version = true;
  resp->minor_version = VERSION_MINOR;
  resp->has_patch_version = true;
  resp->patch_version = VERSION_PATCH;
  resp->has_device_id = true;
  strlcpy(resp->device_id, config_uuid_str, sizeof(resp->device_id));
  resp->has_pin_protection = true;
  resp->pin_protection = config_hasPin();
  resp->has_passphrase_protection = true;
  config_getPassphraseProtection(&(resp->passphrase_protection));
#ifdef SCM_REVISION
  int len = sizeof(SCM_REVISION) - 1;
  resp->has_revision = true;
  memcpy(resp->revision.bytes, SCM_REVISION, len);
  resp->revision.size = len;
#endif
  resp->has_bootloader_hash = true;
  resp->bootloader_hash.size =
      memory_bootloader_hash(resp->bootloader_hash.bytes);

  resp->has_language =
      config_getLanguage(resp->language, sizeof(resp->language));
  resp->has_label = config_getLabel(resp->label, sizeof(resp->label));
  resp->has_initialized = true;
  resp->initialized = config_isInitialized();
  resp->has_imported = config_getImported(&(resp->imported));
  resp->has_pin_cached = true;
  resp->pin_cached = session_isUnlocked() && config_hasPin();
  resp->has_needs_backup = true;
  config_getNeedsBackup(&(resp->needs_backup));
  resp->has_unfinished_backup = true;
  config_getUnfinishedBackup(&(resp->unfinished_backup));
  resp->has_no_backup = true;
  config_getNoBackup(&(resp->no_backup));
  resp->has_flags = config_getFlags(&(resp->flags));
  resp->has_model = true;
  strlcpy(resp->model, "1", sizeof(resp->model));
  if (session_isUnlocked()) {
    resp->has_wipe_code_protection = true;
    resp->wipe_code_protection = config_hasWipeCode();
  }

#if BITCOIN_ONLY
  resp->capabilities_count = 2;
  resp->capabilities[0] = Capability_Capability_Bitcoin;
  resp->capabilities[1] = Capability_Capability_Crypto;
#else
  resp->capabilities_count = 8;
  resp->capabilities[0] = Capability_Capability_Bitcoin;
  resp->capabilities[1] = Capability_Capability_Bitcoin_like;
  resp->capabilities[2] = Capability_Capability_Crypto;
  resp->capabilities[3] = Capability_Capability_Ethereum;
  resp->capabilities[4] = Capability_Capability_Lisk;
  resp->capabilities[5] = Capability_Capability_NEM;
  resp->capabilities[6] = Capability_Capability_Stellar;
  resp->capabilities[7] = Capability_Capability_U2F;
#endif
  if (ble_name_state()) {
    resp->has_ble_name = true;
    strlcpy(resp->ble_name, ble_get_name(), sizeof(resp->ble_name));
  }
  if (ble_ver_state()) {
    resp->has_ble_ver = true;
    strlcpy(resp->ble_ver, ble_get_ver(), sizeof(resp->ble_ver));
  }
  if (ble_switch_state()) {
    resp->has_ble_enable = true;
    resp->ble_enable = ble_get_switch();
  }
  resp->has_se_enable = true;
  resp->se_enable = config_getWhetherUseSE();
  return resp;
}

void fsm_msgInitialize(const Initialize *msg) {
  recovery_abort();
  signing_abort();

  uint8_t *session_id;
  if (msg && msg->has_session_id) {
    session_id = session_startSession(msg->session_id.bytes);
  } else {
    session_id = session_startSession(NULL);
  }

  RESP_INIT(Features);
  get_features(resp);

  resp->has_session_id = true;
  memcpy(resp->session_id.bytes, session_id, sizeof(resp->session_id.bytes));
  resp->session_id.size = sizeof(resp->session_id.bytes);

  layoutHome();
  msg_write(MessageType_MessageType_Features, resp);
}

void fsm_msgGetFeatures(const GetFeatures *msg) {
  (void)msg;
  RESP_INIT(Features);
  get_features(resp);
  msg_write(MessageType_MessageType_Features, resp);
}

void fsm_msgPing(const Ping *msg) {
  RESP_INIT(Success);

  if (msg->has_button_protection && msg->button_protection) {
    layoutDialogSwipe(&bmp_icon_question, _("Cancel"), _("Confirm"), NULL,
                      _("Do you really want to"), _("answer to ping?"), NULL,
                      NULL, NULL, NULL);
    if (!protectButton(ButtonRequestType_ButtonRequest_ProtectCall, false)) {
      fsm_sendFailure(FailureType_Failure_ActionCancelled, NULL);
      layoutHome();
      return;
    }
  }

  if (msg->has_message) {
    resp->has_message = true;
    memcpy(&(resp->message), &(msg->message), sizeof(resp->message));
  }
  msg_write(MessageType_MessageType_Success, resp);
  layoutHome();
}

void fsm_msgChangePin(const ChangePin *msg) {
  CHECK_INITIALIZED

  bool removal = msg->has_remove && msg->remove;
  if (removal) {
    if (config_hasPin()) {
      if (ui_language) {
        layoutDialogSwipe_zh(&bmp_icon_question, "取消", "确认", NULL,
                             "移除PIN码", NULL, NULL, NULL);
      } else {
        layoutDialogSwipe(&bmp_icon_question, _("Cancel"), _("Confirm"), NULL,
                          _("Do you really want to"), _("remove current PIN?"),
                          NULL, NULL, NULL, NULL);
      }

    } else {
      fsm_sendSuccess(_("PIN removed"));
      return;
    }
  } else {
    if (config_hasPin()) {
      if (ui_language) {
        layoutDialogSwipe_zh(&bmp_icon_question, "取消", "确认", NULL,
                             "修改PIN码", NULL, NULL, NULL);
      } else {
        layoutDialogSwipe(&bmp_icon_question, _("Cancel"), _("Confirm"), NULL,
                          _("Do you really want to"), _("change current PIN?"),
                          NULL, NULL, NULL, NULL);
      }

    } else {
      if (ui_language) {
        layoutDialogSwipe_zh(&bmp_icon_question, "取消", "确认", NULL,
                             "设置PIN码", NULL, NULL, NULL);
      } else {
        layoutDialogSwipe(&bmp_icon_question, _("Cancel"), _("Confirm"), NULL,
                          _("Do you really want to"), _("set new PIN?"), NULL,
                          NULL, NULL, NULL);
      }
    }
  }
  if (!protectButton(ButtonRequestType_ButtonRequest_ProtectCall, false)) {
    fsm_sendFailure(FailureType_Failure_ActionCancelled, NULL);
    layoutHome();
    return;
  }

  if (protectChangePin(false, removal)) {
    i2c_set_wait(false);
    if (removal) {
      fsm_sendSuccess(_("PIN removed"));
    } else {
      fsm_sendSuccess(_("PIN changed"));
    }
  }

  layoutHome();
}

void fsm_msgChangeWipeCode(const ChangeWipeCode *msg) {
  CHECK_INITIALIZED
  if (g_bIsBixinAPP) {
    CHECK_PIN
  }

  bool removal = msg->has_remove && msg->remove;
  bool has_wipe_code = config_hasWipeCode();

  if (removal) {
    // Note that if storage is locked, then config_hasWipeCode() returns false.
    if (has_wipe_code || !session_isUnlocked()) {
      layoutDialogSwipe(&bmp_icon_question, _("Cancel"), _("Confirm"), NULL,
                        _("Do you really want to"), _("disable wipe code"),
                        _("protection?"), NULL, NULL, NULL);
    } else {
      fsm_sendSuccess(_("Wipe code removed"));
      return;
    }
  } else {
    if (has_wipe_code) {
      layoutDialogSwipe(&bmp_icon_question, _("Cancel"), _("Confirm"), NULL,
                        _("Do you really want to"), _("change the current"),
                        _("wipe code?"), NULL, NULL, NULL);
    } else {
      layoutDialogSwipe(&bmp_icon_question, _("Cancel"), _("Confirm"), NULL,
                        _("Do you really want to"), _("set a new wipe code?"),
                        NULL, NULL, NULL, NULL);
    }
  }
  if (!protectButton(ButtonRequestType_ButtonRequest_ProtectCall, false)) {
    fsm_sendFailure(FailureType_Failure_ActionCancelled, NULL);
    layoutHome();
    return;
  }

  if (protectChangeWipeCode(removal)) {
    if (removal) {
      fsm_sendSuccess(_("Wipe code removed"));
    } else if (has_wipe_code) {
      fsm_sendSuccess(_("Wipe code changed"));
    } else {
      fsm_sendSuccess(_("Wipe code set"));
    }
  }

  layoutHome();
}

void fsm_msgWipeDevice(const WipeDevice *msg) {
  if (g_bIsBixinAPP) {
    CHECK_PIN_UNCACHED
  }
  (void)msg;
  if (ui_language) {
    layoutDialogSwipe_zh(&bmp_icon_question, "取消", "确认", NULL, "删除钱包",
                         "所有数据将丢失", NULL, NULL);
  } else {
    layoutDialogSwipe(&bmp_icon_question, _("Cancel"), _("Confirm"), NULL,
                      _("Do you really want to"), _("wipe the device?"), NULL,
                      _("All data will be lost."), NULL, NULL);
  }

  if (!protectButton(ButtonRequestType_ButtonRequest_WipeDevice, false)) {
    i2c_set_wait(false);
    fsm_sendFailure(FailureType_Failure_ActionCancelled, NULL);
    layoutHome();
    return;
  }
  config_wipe();
  // the following does not work on Mac anyway :-/ Linux/Windows are fine, so it
  // is not needed usbReconnect(); // force re-enumeration because of the serial
  // number change
  i2c_set_wait(false);
  fsm_sendSuccess(_("Device wiped"));
  layoutHome();
}

void fsm_msgGetEntropy(const GetEntropy *msg) {
#if !DEBUG_RNG
  layoutDialogSwipe(&bmp_icon_question, _("Cancel"), _("Confirm"), NULL,
                    _("Do you really want to"), _("send entropy?"), NULL, NULL,
                    NULL, NULL);
  if (!protectButton(ButtonRequestType_ButtonRequest_ProtectCall, false)) {
    fsm_sendFailure(FailureType_Failure_ActionCancelled, NULL);
    layoutHome();
    return;
  }
#endif
  RESP_INIT(Entropy);
  uint32_t len = msg->size;
  if (len > 1024) {
    len = 1024;
  }
  resp->entropy.size = len;
  random_buffer(resp->entropy.bytes, len);
  msg_write(MessageType_MessageType_Entropy, resp);
  layoutHome();
}

#if DEBUG_LINK

void fsm_msgLoadDevice(const LoadDevice *msg) {
  CHECK_PIN

  CHECK_NOT_INITIALIZED

  layoutDialogSwipe(&bmp_icon_question, _("Cancel"), _("I take the risk"), NULL,
                    _("Loading private seed"), _("is not recommended."),
                    _("Continue only if you"), _("know what you are"),
                    _("doing!"), NULL);
  if (!protectButton(ButtonRequestType_ButtonRequest_ProtectCall, false)) {
    fsm_sendFailure(FailureType_Failure_ActionCancelled, NULL);
    layoutHome();
    return;
  }

  if (msg->mnemonics_count && !(msg->has_skip_checksum && msg->skip_checksum)) {
    if (!mnemonic_check(msg->mnemonics[0])) {
      fsm_sendFailure(FailureType_Failure_DataError,
                      _("Mnemonic with wrong checksum provided"));
      layoutHome();
      return;
    }
  }

  config_loadDevice(msg);
  fsm_sendSuccess(_("Device loaded"));
  layoutHome();
}

#endif

void fsm_msgResetDevice(const ResetDevice *msg) {
  if (g_bIsBixinAPP && config_getDeviceState() == DeviceState_ResetSetPin) {
  } else {
    CHECK_PIN
  }

  CHECK_NOT_INITIALIZED

  CHECK_PARAM(!msg->has_strength || msg->strength == 128 ||
                  msg->strength == 192 || msg->strength == 256,
              _("Invalid seed strength"));

  reset_init(msg->has_display_random && msg->display_random,
             msg->has_strength ? msg->strength : 128,
             msg->has_passphrase_protection && msg->passphrase_protection,
             msg->has_pin_protection && msg->pin_protection,
             msg->has_language ? msg->language : 0,
             msg->has_label ? msg->label : 0,
             msg->has_u2f_counter ? msg->u2f_counter : 0,
             msg->has_skip_backup ? msg->skip_backup : false,
             msg->has_no_backup ? msg->no_backup : false);
}

void fsm_msgEntropyAck(const EntropyAck *msg) {
  if (msg->has_entropy) {
    reset_entropy(msg->entropy.bytes, msg->entropy.size);
  } else {
    reset_entropy(0, 0);
  }
}

void fsm_msgBackupDevice(const BackupDevice *msg) {
  (void)msg;

  CHECK_INITIALIZED

  CHECK_PIN_UNCACHED

  char mnemonic[MAX_MNEMONIC_LEN + 1];
  if (config_getMnemonic(mnemonic, sizeof(mnemonic))) {
    reset_backup(true, mnemonic);
  }
  memzero(mnemonic, sizeof(mnemonic));
}

void fsm_msgCancel(const Cancel *msg) {
  (void)msg;
  recovery_abort();
  signing_abort();
#if !BITCOIN_ONLY
  ethereum_signing_abort();
#endif
  fsm_sendFailure(FailureType_Failure_ActionCancelled, NULL);
}

void fsm_msgClearSession(const ClearSession *msg) {
  (void)msg;
  // we do not actually clear the session, we just lock it
  // TODO: the message should be called LockSession see #819
  config_lockDevice();
  layoutScreensaver();
  fsm_sendSuccess(_("Session cleared"));
}

bool fsm_getLang(const ApplySettings *msg) {
  if (!strcmp(msg->language, "zh") || !strcmp(msg->language, "chinese"))
    return true;
  else
    return false;
}

void fsm_msgApplySettings(const ApplySettings *msg) {
  CHECK_PARAM(
      !msg->has_passphrase_always_on_device,
      _("This firmware is incapable of passphrase entry on the device."));

  CHECK_PARAM(msg->has_label || msg->has_language || msg->has_use_passphrase ||
                  msg->has_homescreen || msg->has_auto_lock_delay_ms ||
                  msg->has_fastpay_pin || msg->has_use_ble || msg->has_use_se ||
                  msg->has_fastpay_confirm || msg->has_fastpay_money_limit ||
                  msg->has_fastpay_times || msg->has_is_bixinapp,
              _("No setting provided"));

  if (!msg->has_is_bixinapp) CHECK_PIN

  if (msg->has_is_bixinapp) {
    if (msg->has_label || msg->has_language || msg->has_use_passphrase ||
        msg->has_homescreen || msg->has_auto_lock_delay_ms ||
        msg->has_fastpay_pin || msg->has_use_ble || msg->has_use_se ||
        msg->has_fastpay_confirm || msg->has_fastpay_money_limit ||
        msg->has_fastpay_times) {
      fsm_sendFailure(FailureType_Failure_ActionCancelled,
                      "you should set bixin_app flag only");
      layoutHome();
      return;
    }
  }

  if (msg->has_label) {
    if (ui_language) {
      layoutDialogSwipe_zh(&bmp_icon_question, "取消", "确认", NULL,
                           "设置钱包名称为:", msg->label, "?", NULL);
    } else {
      layoutDialogSwipe(&bmp_icon_question, _("Cancel"), _("Confirm"), NULL,
                        _("Do you really want to"), _("change name to"),
                        msg->label, "?", NULL, NULL);
    }
    if (!protectButton(ButtonRequestType_ButtonRequest_ProtectCall, false)) {
      fsm_sendFailure(FailureType_Failure_ActionCancelled, NULL);
      layoutHome();
      return;
    }
  }
  if (msg->has_language) {
    if (ui_language) {
      layoutDialogSwipe_zh(&bmp_icon_question, "取消", "确认", NULL,
                           "设置语言为:", (fsm_getLang(msg) ? "中文" : "英语"),
                           NULL, NULL);
    } else {
      layoutDialogSwipe(&bmp_icon_question, _("Cancel"), _("Confirm"), NULL,
                        _("Do you really want to"), _("change language to"),
                        msg->language, "?", NULL, NULL);
    }
    if (!protectButton(ButtonRequestType_ButtonRequest_ProtectCall, false)) {
      fsm_sendFailure(FailureType_Failure_ActionCancelled, NULL);
      layoutHome();
      return;
    }
  }
  if (msg->has_use_passphrase) {
    if (ui_language) {
      layoutDialogSwipe_zh(&bmp_icon_question, "取消", "确认", NULL,
                           msg->use_passphrase ? "使用密语" : "禁用密语",
                           "加密?", NULL, NULL);
    } else {
      layoutDialogSwipe(&bmp_icon_question, _("Cancel"), _("Confirm"), NULL,
                        _("Do you really want to"),
                        msg->use_passphrase ? _("enable passphrase")
                                            : _("disable passphrase"),
                        _("protection?"), NULL, NULL, NULL);
    }
    if (!protectButton(ButtonRequestType_ButtonRequest_ProtectCall, false)) {
      fsm_sendFailure(FailureType_Failure_ActionCancelled, NULL);
      layoutHome();
      return;
    }
  }
  if (msg->has_homescreen) {
    if (ui_language) {
      layoutDialogSwipe_zh(&bmp_icon_question, "取消", "确认", NULL,
                           "修改屏幕显示", NULL, NULL, NULL);
    } else {
      layoutDialogSwipe(&bmp_icon_question, _("Cancel"), _("Confirm"), NULL,
                        _("Do you really want to"), _("change the home"),
                        _("screen?"), NULL, NULL, NULL);
    }
    if (!protectButton(ButtonRequestType_ButtonRequest_ProtectCall, false)) {
      fsm_sendFailure(FailureType_Failure_ActionCancelled, NULL);
      layoutHome();
      return;
    }
  }

  if (msg->has_auto_lock_delay_ms) {
    layoutDialogSwipe(&bmp_icon_question, _("Cancel"), _("Confirm"), NULL,
                      _("Do you really want to"), _("change auto-lock"),
                      _("delay?"), NULL, NULL, NULL);
    if (!protectButton(ButtonRequestType_ButtonRequest_ProtectCall, false)) {
      fsm_sendFailure(FailureType_Failure_ActionCancelled, NULL);
      layoutHome();
      return;
    }
  }
  if ((msg->has_fastpay_pin) || (msg->has_fastpay_confirm) ||
      (msg->has_fastpay_money_limit) || (msg->has_fastpay_times)) {
    layoutDialogSwipe(&bmp_icon_question, _("Cancel"), _("Confirm"), NULL,
                      _("Do you really want to"), _("change fastpay settings"),
                      NULL, NULL, NULL, NULL);
    if (!protectButton(ButtonRequestType_ButtonRequest_ProtectCall, false)) {
      fsm_sendFailure(FailureType_Failure_ActionCancelled, NULL);
      layoutHome();
      return;
    }
  }
  if (msg->has_use_ble) {
    layoutDialogSwipe(&bmp_icon_question, _("Cancel"), _("Confirm"), NULL,
                      _("Do you really want to"), _("change bluetooth"),
                      _("status always?"), NULL, NULL, NULL);
    if (!protectButton(ButtonRequestType_ButtonRequest_ProtectCall, false)) {
      fsm_sendFailure(FailureType_Failure_ActionCancelled, NULL);
      layoutHome();
      return;
    }
  }
  if ((msg->has_use_se) && (config_isInitialized())) {
    fsm_sendSuccess(_("Can't change se setting after device initialized"));
    layoutHome();
    return;
  }
  if (msg->has_label) {
    config_setLabel(msg->label);
  }
  if (msg->has_language) {
    config_setLanguage(msg->language);
  }
  if (msg->has_use_passphrase) {
    config_setPassphraseProtection(msg->use_passphrase);
  }
  if (msg->has_homescreen) {
    config_setHomescreen(msg->homescreen.bytes, msg->homescreen.size);
  }
  if (msg->has_auto_lock_delay_ms) {
    config_setAutoLockDelayMs(msg->auto_lock_delay_ms);
  }
  if (msg->has_fastpay_pin) {
    // not allowed set true if not use se
    if (!g_bSelectSEFlag) {
      config_setFastPayPinFlag(false);
    } else {
      config_setFastPayPinFlag(msg->fastpay_pin);
    }
  }
  if (msg->has_fastpay_confirm) {
    config_setFastPayConfirmFlag(msg->fastpay_confirm);
  }
  if (msg->has_fastpay_money_limit) {
    config_setFastPayMoneyLimt(msg->fastpay_money_limit);
  }
  if (msg->has_fastpay_times) {
    config_setFastPayTimes(msg->fastpay_times);
  }
  if (msg->has_use_ble) {
    config_setBleTrans(msg->use_ble);
  }
  if (msg->has_use_se) {
    config_setWhetherUseSE(msg->use_se);
  }
  if (msg->has_is_bixinapp) {
    config_setIsBixinAPP();
  }
  fsm_sendSuccess(_("Settings applied"));
  layoutHome();
}

void fsm_msgApplyFlags(const ApplyFlags *msg) {
  CHECK_PIN

  if (msg->has_flags) {
    config_applyFlags(msg->flags);
  }
  fsm_sendSuccess(_("Flags applied"));
}

void fsm_msgRecoveryDevice(const RecoveryDevice *msg) {
  CHECK_PIN_UNCACHED

  const bool dry_run = msg->has_dry_run ? msg->dry_run : false;
  if (!dry_run) {
    CHECK_NOT_INITIALIZED
  } else {
    CHECK_INITIALIZED
    CHECK_PARAM(!msg->has_passphrase_protection && !msg->has_pin_protection &&
                    !msg->has_language && !msg->has_label &&
                    !msg->has_u2f_counter,
                _("Forbidden field set in dry-run"))
  }

  CHECK_PARAM(!msg->has_word_count || msg->word_count == 12 ||
                  msg->word_count == 18 || msg->word_count == 24,
              _("Invalid word count"));

  recovery_init(msg->has_word_count ? msg->word_count : 12,
                msg->has_passphrase_protection && msg->passphrase_protection,
                msg->has_pin_protection && msg->pin_protection,
                msg->has_language ? msg->language : 0,
                msg->has_label ? msg->label : 0,
                msg->has_enforce_wordlist && msg->enforce_wordlist,
                msg->has_type ? msg->type : 0,
                msg->has_u2f_counter ? msg->u2f_counter : 0, dry_run);
}

void fsm_msgWordAck(const WordAck *msg) { recovery_word(msg->word); }

void fsm_msgSetU2FCounter(const SetU2FCounter *msg) {
  layoutDialogSwipe(&bmp_icon_question, _("Cancel"), _("Confirm"), NULL,
                    _("Do you want to set"), _("the U2F counter?"), NULL, NULL,
                    NULL, NULL);
  if (!protectButton(ButtonRequestType_ButtonRequest_ProtectCall, false)) {
    fsm_sendFailure(FailureType_Failure_ActionCancelled, NULL);
    layoutHome();
    return;
  }
  config_setU2FCounter(msg->u2f_counter);
  fsm_sendSuccess(_("U2F counter set"));
  layoutHome();
}

void fsm_msgGetNextU2FCounter() {
  layoutDialogSwipe(&bmp_icon_question, _("Cancel"), _("Confirm"), NULL,
                    _("Do you want to"), _("increase and retrieve"),
                    _("the U2F counter?"), NULL, NULL, NULL);
  if (!protectButton(ButtonRequestType_ButtonRequest_ProtectCall, false)) {
    fsm_sendFailure(FailureType_Failure_ActionCancelled, NULL);
    layoutHome();
    return;
  }
  uint32_t counter = config_nextU2FCounter();

  RESP_INIT(NextU2FCounter);
  resp->has_u2f_counter = true;
  resp->u2f_counter = counter;
  msg_write(MessageType_MessageType_NextU2FCounter, resp);
  layoutHome();
}

void fsm_msgBixinSeedOperate(const BixinSeedOperate *msg) {
  uint8_t ucBuf[65], i = 0;
  uint32_t uiTemp;

  if (msg->type == SeedRequestType_SeedRequestType_Gen) {
    CHECK_PIN
    CHECK_NOT_INITIALIZED
    for (i = 0; i < 16; i++) {
      uiTemp = random32();
      memcpy(ucBuf + 1 + i * 4, &uiTemp, 4);
    }
    ucBuf[0] = config_setSeedsExportFlag(ExportType_SeedEncExportType_YES);
    if (!config_setSeedsBytes(ucBuf, 65)) {
      fsm_sendFailure(FailureType_Failure_NotInitialized, NULL);
      layoutHome();
      return;
    }
    fsm_sendSuccess(_("device initialied success"));
    layoutHome();
    return;
  } else if (msg->type == SeedRequestType_SeedRequestType_EncExport) {
    RESP_INIT(BixinOutMessageSE);
    CHECK_PIN
    CHECK_INITIALIZED
    if (!config_SeedsEncExportBytes(
            (BixinOutMessageSE_outmessage_t *)(&resp->outmessage))) {
      fsm_sendFailure(FailureType_Failure_NotInitialized, NULL);
      layoutHome();
      return;
    }
    resp->has_outmessage = true;
    msg_write(MessageType_MessageType_BixinOutMessageSE, resp);
    layoutHome();
    return;
  } else if (msg->type == SeedRequestType_SeedRequestType_EncImport) {
    CHECK_PIN
    CHECK_NOT_INITIALIZED
    if (msg->has_seed_importData) {
      if (config_SeedsEncImportBytes(
              (BixinSeedOperate_seed_importData_t *)(&msg->seed_importData))) {
        fsm_sendSuccess(_("seed import success"));
        layoutHome();
        return;
      }
    }
  }
  fsm_sendFailure(FailureType_Failure_DataError, NULL);
  layoutHome();
}

void fsm_msgBixinReboot(const BixinReboot *msg) {
  (void)msg;
  fsm_sendSuccess(_("reboot start"));
  usbPoll();  // send response before reboot
#if !EMULATOR
  sys_backtoboot();
#endif
}

void fsm_msgBixinMessageSE(const BixinMessageSE *msg) {
  RESP_INIT(BixinOutMessageSE);

  if (false == config_getMessageSE(
                   (BixinMessageSE_inputmessage_t *)(&msg->inputmessage),
                   (BixinOutMessageSE_outmessage_t *)(&resp->outmessage))) {
    fsm_sendFailure(FailureType_Failure_UnexpectedMessage, NULL);
    layoutHome();
    return;
  }
  resp->has_outmessage = true;
  msg_write(MessageType_MessageType_BixinOutMessageSE, resp);
  layoutHome();
  return;
}

void fsm_msgBixinBackupRequest(const BixinBackupRequest *msg) {
  (void)msg;
  CHECK_INITIALIZED
  CHECK_PIN_UNCACHED
  RESP_INIT(BixinBackupAck);
  resp->data.size -= 4;  // 4bytes header,rfu
  if (g_bSelectSEFlag) {
    resp->data.bytes[0] = 0x00;
    if (false == se_backup((uint8_t *)resp->data.bytes + 4, &resp->data.size)) {
      fsm_sendFailure(FailureType_Failure_UnexpectedMessage, NULL);
      layoutHome();
      return;
    }
    resp->data.size += 4;
  } else {
    resp->data.bytes[0] = 0x01;
    config_getMnemonic((char *)(resp->data.bytes + 4), resp->data.size - 4);
    resp->data.size = strlen((char *)(resp->data.bytes + 4)) + 4;
  }
  config_setNeedsBackup(false);
  msg_write(MessageType_MessageType_BixinBackupAck, resp);
  layoutHome();
  return;
}

void fsm_msgBixinRestoreRequest(const BixinRestoreRequest *msg) {
  CHECK_PIN
  CHECK_NOT_INITIALIZED
  // restore in se
  if (msg->data.bytes[0] == 0x00) {
    if (!config_getWhetherUseSE()) {
      config_setWhetherUseSE(true);
    }
    if (false ==
        se_restore((uint8_t *)(msg->data.bytes + 4), msg->data.size - 4)) {
      fsm_sendFailure(FailureType_Failure_DataError,
                      "Restor seed in se failure");
    } else {
      fsm_sendSuccess(_("device initialied success"));
    }

  } else if (msg->data.bytes[0] == 0x01) {
    if (config_getWhetherUseSE()) {
      config_setWhetherUseSE(false);
    }
    if (config_setMnemonic((char *)(msg->data.bytes + 4))) {
      fsm_sendSuccess(_("Device successfully initialized"));
    } else {
      fsm_sendFailure(FailureType_Failure_ProcessError,
                      _("Failed to store mnemonic"));
    }

  } else {
    fsm_sendFailure(FailureType_Failure_DataError, "Restor data format error");
  }
  layoutHome();
  return;
};

void fsm_msgBixinVerifyDeviceRequest(const BixinVerifyDeviceRequest *msg) {
  RESP_INIT(BixinVerifyDeviceAck);
  resp->cert.size = 1024;
  resp->signature.size = 512;
  if (false == se_verify((uint8_t *)msg->data.bytes, msg->data.size, 1024,
                         resp->cert.bytes, &resp->cert.size,
                         resp->signature.bytes, &resp->signature.size)) {
    fsm_sendFailure(FailureType_Failure_UnexpectedMessage, NULL);
    layoutHome();
    return;
  }
  msg_write(MessageType_MessageType_BixinVerifyDeviceAck, resp);
  layoutHome();
  return;
};
