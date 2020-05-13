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

#include <string.h>

#include "bootloader.h"
#include "ecdsa.h"
#include "memory.h"
#include "memzero.h"
#include "secp256k1.h"
#include "sha2.h"
#include "signatures.h"

const uint32_t FIRMWARE_MAGIC_OLD = 0x525a5254;  // TRZR
const uint32_t FIRMWARE_MAGIC_NEW = 0x465a5254;  // TRZF
const uint32_t FIRMWARE_MAGIC_BLE = 0x33383235;  // 5283

#define PUBKEYS 5

static const uint8_t * const pubkey[PUBKEYS] = {
	(const uint8_t *)"\x04\xd5\x71\xb7\xf1\x48\xc5\xe4\x23\x2c\x38\x14\xf7\x77\xd8\xfa\xea\xf1\xa8\x42\x16\xc7\x8d\x56\x9b\x71\x04\x1f\xfc\x76\x8a\x5b\x2d\x81\x0f\xc3\xbb\x13\x4d\xd0\x26\xb5\x7e\x65\x00\x52\x75\xae\xde\xf4\x3e\x15\x5f\x48\xfc\x11\xa3\x2e\xc7\x90\xa9\x33\x12\xbd\x58",
	(const uint8_t *)"\x04\x63\x27\x9c\x0c\x08\x66\xe5\x0c\x05\xc7\x99\xd3\x2b\xd6\xba\xb0\x18\x8b\x6d\xe0\x65\x36\xd1\x10\x9d\x2e\xd9\xce\x76\xcb\x33\x5c\x49\x0e\x55\xae\xe1\x0c\xc9\x01\x21\x51\x32\xe8\x53\x09\x7d\x54\x32\xed\xa0\x6b\x79\x20\x73\xbd\x77\x40\xc9\x4c\xe4\x51\x6c\xb1",
	(const uint8_t *)"\x04\x43\xae\xdb\xb6\xf7\xe7\x1c\x56\x3f\x8e\xd2\xef\x64\xec\x99\x81\x48\x25\x19\xe7\xef\x4f\x4a\xa9\x8b\x27\x85\x4e\x8c\x49\x12\x6d\x49\x56\xd3\x00\xab\x45\xfd\xc3\x4c\xd2\x6b\xc8\x71\x0d\xe0\xa3\x1d\xbd\xf6\xde\x74\x35\xfd\x0b\x49\x2b\xe7\x0a\xc7\x5f\xde\x58",
	(const uint8_t *)"\x04\x87\x7c\x39\xfd\x7c\x62\x23\x7e\x03\x82\x35\xe9\xc0\x75\xda\xb2\x61\x63\x0f\x78\xee\xb8\xed\xb9\x24\x87\x15\x9f\xff\xed\xfd\xf6\x04\x6c\x6f\x8b\x88\x1f\xa4\x07\xc4\xa4\xce\x6c\x28\xde\x0b\x19\xc1\xf4\xe2\x9f\x1f\xcb\xc5\xa5\x8f\xfd\x14\x32\xa3\xe0\x93\x8a",
	(const uint8_t *)"\x04\x73\x84\xc5\x1a\xe8\x1a\xdd\x0a\x52\x3a\xdb\xb1\x86\xc9\x1b\x90\x6f\xfb\x64\xc2\xc7\x65\x80\x2b\xf2\x6d\xbd\x13\xbd\xf1\x2c\x31\x9e\x80\xc2\x21\x3a\x13\x6c\x8e\xe0\x3d\x78\x74\xfd\x22\xb7\x0d\x68\xe7\xde\xe4\x69\xde\xcf\xbb\xb5\x10\xee\x9a\x46\x0c\xda\x45",
};

static const uint8_t * const pubkey_Bixin[PUBKEYS] = {
	(const uint8_t *)"\x04\xad\x90\x35\xd6\x7a\xc4\x79\x5c\x91\x3c\x45\x2d\x25\x15\x6f\x0b\x09\x4c\x34\xf6\x56\xa2\x49\xb9\x4d\x8d\x66\x19\xab\x0d\x92\xb1\xe8\xbc\xc3\x28\xbd\xc8\x33\xb9\xb5\x1c\xa3\x1b\xfd\x01\x36\x61\x51\x53\xf9\x3a\xba\x46\xd0\x2a\xb5\xd9\x25\xf4\xf3\x64\xc6\x66",
	(const uint8_t *)"\x04\x95\xb8\x3d\xa4\x42\xc6\x89\xbd\xa8\x2e\x9f\x95\x43\x81\x1f\xec\x2f\x58\x33\x4f\x5c\x76\x36\x1f\x5a\x49\xfb\xb6\x63\x4a\x81\x15\x2c\x6d\xa7\xb8\xa1\x78\x2c\xca\xa9\x28\x7e\xc7\xa8\xef\xe9\xd8\xbb\xa7\xd9\x01\x80\xf7\xb7\x19\xc0\x17\xd8\x04\xd6\x1c\x3d\x5d",
	(const uint8_t *)"\x04\xe0\xd5\xef\x94\xcf\x95\x0b\x9f\x85\x5f\xb5\x52\x67\x64\xdc\x28\xd2\xd9\x65\x82\xc1\xca\x1a\xd3\x9c\xab\xab\x65\x3e\x61\x98\xf8\x0e\x64\x95\xe5\x36\xb8\xbc\xe3\x78\x57\xda\xfc\x5a\x51\x95\x26\x24\xab\x08\x3c\x33\x16\x8e\xe3\xed\x83\xe0\x36\xde\xfa\xb8\x5c",
	(const uint8_t *)"\x04\xc9\xec\x74\x0c\xb3\x32\x81\x65\xdc\x3f\xdb\x93\xa5\x4e\x70\x75\x56\x9b\x7a\x54\xcd\xee\xc0\x21\x1f\xa9\xd9\x52\xc6\x64\x79\x60\xbd\x95\xf9\x94\xc8\x17\x45\x88\x92\xd5\xdb\x6e\xb3\x4d\xa7\x6c\xe0\x3c\x9c\x04\xa4\x32\x5c\x27\x52\x64\x75\x90\xb1\xa4\xf3\x65",
	(const uint8_t *)"\x04\xd2\x08\xab\xa7\x9b\x6f\x60\xf1\x78\x60\x81\x67\xdd\xb7\x77\x86\x0a\x81\x55\x02\x2e\x28\xf1\x20\xa4\x16\x30\x86\x9a\x4e\x0f\x0d\x16\x98\xa4\x0e\xed\x2a\xed\xf7\x48\x46\xe1\xe4\x01\xce\xe6\xfd\xb1\xe8\x11\x16\xd1\x4c\xd2\x97\x12\x70\xf2\xcb\x0c\x56\x2c\x2e",
};

#define SIGNATURES 3

#define FLASH_META_START 0x08008000
#define FLASH_META_CODELEN (FLASH_META_START + 0x0004)
#define FLASH_META_SIGINDEX1 (FLASH_META_START + 0x0008)
#define FLASH_META_SIGINDEX2 (FLASH_META_START + 0x0009)
#define FLASH_META_SIGINDEX3 (FLASH_META_START + 0x000A)
#define FLASH_OLD_APP_START 0x08010000
#define FLASH_META_SIG1 (FLASH_META_START + 0x0040)
#define FLASH_META_SIG2 (FLASH_META_START + 0x0080)
#define FLASH_META_SIG3 (FLASH_META_START + 0x00C0)

bool firmware_present_old(void) {
  if (memcmp(FLASH_PTR(FLASH_META_START), &FIRMWARE_MAGIC_OLD,
             4)) {  // magic does not match
    return false;
  }
  if (*((const uint32_t *)FLASH_PTR(FLASH_META_CODELEN)) <
      8192) {  // firmware reports smaller size than 8192
    return false;
  }
  if (*((const uint32_t *)FLASH_PTR(FLASH_META_CODELEN)) >
      FLASH_APP_LEN) {  // firmware reports bigger size than flash size
    return false;
  }

  return true;
}

int signatures_old_ok(void) {
  const uint32_t codelen = *((const uint32_t *)FLASH_META_CODELEN);
  const uint8_t sigindex1 = *((const uint8_t *)FLASH_META_SIGINDEX1);
  const uint8_t sigindex2 = *((const uint8_t *)FLASH_META_SIGINDEX2);
  const uint8_t sigindex3 = *((const uint8_t *)FLASH_META_SIGINDEX3);

  if (codelen > FLASH_APP_LEN) {
    return false;
  }

  uint8_t hash[32] = {0};
  sha256_Raw(FLASH_PTR(FLASH_OLD_APP_START), codelen, hash);

  if (sigindex1 < 1 || sigindex1 > PUBKEYS) return SIG_FAIL;  // invalid index
  if (sigindex2 < 1 || sigindex2 > PUBKEYS) return SIG_FAIL;  // invalid index
  if (sigindex3 < 1 || sigindex3 > PUBKEYS) return SIG_FAIL;  // invalid index

  if (sigindex1 == sigindex2) return SIG_FAIL;  // duplicate use
  if (sigindex1 == sigindex3) return SIG_FAIL;  // duplicate use
  if (sigindex2 == sigindex3) return SIG_FAIL;  // duplicate use

  if (0 != ecdsa_verify_digest(&secp256k1, pubkey_Bixin[sigindex1 - 1],
                               (const uint8_t *)FLASH_META_SIG1,
                               hash)) {  // failure
    if (0 != ecdsa_verify_digest(&secp256k1, pubkey[sigindex1 - 1],
                                 (const uint8_t *)FLASH_META_SIG1,
                                 hash)) {  // failure
      return SIG_FAIL;
    }
  }
  if (0 != ecdsa_verify_digest(&secp256k1, pubkey_Bixin[sigindex2 - 1],
                               (const uint8_t *)FLASH_META_SIG2,
                               hash)) {  // failure
    if (0 != ecdsa_verify_digest(&secp256k1, pubkey[sigindex2 - 1],
                                 (const uint8_t *)FLASH_META_SIG2,
                                 hash)) {  // failure
      return SIG_FAIL;
    }
  }
  if (0 != ecdsa_verify_digest(&secp256k1, pubkey_Bixin[sigindex3 - 1],
                               (const uint8_t *)FLASH_META_SIG3,
                               hash)) {  // failture
    if (0 != ecdsa_verify_digest(&secp256k1, pubkey[sigindex3 - 1],
                                 (const uint8_t *)FLASH_META_SIG3,
                                 hash)) {  // failture
      return SIG_FAIL;
    }
  }

  return SIG_OK;
}

void compute_firmware_fingerprint(const image_header *hdr, uint8_t hash[32]) {
  image_header copy = {0};
  memcpy(&copy, hdr, sizeof(image_header));
  memzero(copy.sig1, sizeof(copy.sig1));
  memzero(copy.sig2, sizeof(copy.sig2));
  memzero(copy.sig3, sizeof(copy.sig3));
  copy.sigindex1 = 0;
  copy.sigindex2 = 0;
  copy.sigindex3 = 0;
  sha256_Raw((const uint8_t *)&copy, sizeof(image_header), hash);
}

bool firmware_present_new(void) {
  const image_header *hdr =
      (const image_header *)FLASH_PTR(FLASH_FWHEADER_START);
  if (hdr->magic != FIRMWARE_MAGIC_NEW) return false;
  // we need to ignore hdrlen for now
  // because we keep reset_handler ptr there
  // for compatibility with older bootloaders
  // after this is no longer necessary, let's uncomment the line below:
  // if (hdr->hdrlen != FLASH_FWHEADER_LEN) return false;
  if (hdr->codelen > FLASH_APP_LEN) return false;
  if (hdr->codelen < 4096) return false;

  return true;
}

int signatures_new_ok(const image_header *hdr, uint8_t store_fingerprint[32]) {
  uint8_t hash[32] = {0};
  compute_firmware_fingerprint(hdr, hash);

  if (store_fingerprint) {
    memcpy(store_fingerprint, hash, 32);
  }

  if (hdr->sigindex1 < 1 || hdr->sigindex1 > PUBKEYS)
    return SIG_FAIL;  // invalid index
  if (hdr->sigindex2 < 1 || hdr->sigindex2 > PUBKEYS)
    return SIG_FAIL;  // invalid index
  if (hdr->sigindex3 < 1 || hdr->sigindex3 > PUBKEYS)
    return SIG_FAIL;  // invalid index

  if (hdr->sigindex1 == hdr->sigindex2) return SIG_FAIL;  // duplicate use
  if (hdr->sigindex1 == hdr->sigindex3) return SIG_FAIL;  // duplicate use
  if (hdr->sigindex2 == hdr->sigindex3) return SIG_FAIL;  // duplicate use

  if (0 != ecdsa_verify_digest(&secp256k1, pubkey[hdr->sigindex1 - 1],
                               hdr->sig1, hash)) {  // failure
    if (0 != ecdsa_verify_digest(&secp256k1, pubkey_Bixin[hdr->sigindex1 - 1],
                                 hdr->sig1, hash)) {  // failure
      return SIG_FAIL;
    }
  }
  if (0 != ecdsa_verify_digest(&secp256k1, pubkey[hdr->sigindex2 - 1],
                               hdr->sig2, hash)) {  // failure
    if (0 != ecdsa_verify_digest(&secp256k1, pubkey_Bixin[hdr->sigindex2 - 1],
                                 hdr->sig2, hash)) {  // failure
      return SIG_FAIL;
    }
  }
  if (0 != ecdsa_verify_digest(&secp256k1, pubkey[hdr->sigindex3 - 1],
                               hdr->sig3, hash)) {  // failure
    if (0 != ecdsa_verify_digest(&secp256k1, pubkey_Bixin[hdr->sigindex3 - 1],
                                 hdr->sig3, hash)) {  // failure
      return SIG_FAIL;
    }
  }

  return SIG_OK;
}

int mem_is_empty(const uint8_t *src, uint32_t len) {
  for (uint32_t i = 0; i < len; i++) {
    if (src[i]) return 0;
  }
  return 1;
}

int check_firmware_hashes(const image_header *hdr) {
  uint8_t hash[32] = {0};
  // check hash of the first code chunk
  sha256_Raw(FLASH_PTR(FLASH_APP_START), (64 - 1) * 1024, hash);
  if (0 != memcmp(hash, hdr->hashes, 32)) return SIG_FAIL;
  // check remaining used chunks
  uint32_t total_len = FLASH_FWHEADER_LEN + hdr->codelen;
  int used_chunks = total_len / FW_CHUNK_SIZE;
  if (total_len % FW_CHUNK_SIZE > 0) {
    used_chunks++;
  }
  for (int i = 1; i < used_chunks; i++) {
    sha256_Raw(FLASH_PTR(FLASH_FWHEADER_START + (64 * i) * 1024), 64 * 1024,
               hash);
    if (0 != memcmp(hdr->hashes + 32 * i, hash, 32)) return SIG_FAIL;
  }
  // check unused chunks
  for (int i = used_chunks; i < 16; i++) {
    if (!mem_is_empty(hdr->hashes + 32 * i, 32)) return SIG_FAIL;
  }
  // all OK
  return SIG_OK;
}
