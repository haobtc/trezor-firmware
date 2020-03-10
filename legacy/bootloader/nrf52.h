#ifndef __NRF52_H_DEFINED__
#define __NRF52_H_DEFINED__

#define NVMC_ADDRESS 0x4001E000
#define READY_OFFSET 0x00000400
#define CONFIG_OFFSET 0x00000504
#define ERASEPAGE_OFFSET 0x00000508
#define ERASEALL_OFFSET 0x0000050C
#define ERASEUICR_OFFSET 0x00000514
#define ERASEPAGE_OFFSET 0x00000508

#define FIRMWARE_PIN_ADDRESS 0x00030000

#define NVMCREADY 0x00000001
#define NVMCREN 0x00000000  // enable read
#define NVMCWEN 0x00000001  // enable write
#define NVMCEEN 0x00000002  // enable erase

#define NVMCERASEUICR 0x00000001  // erase app
#define NVMCERASE 0x00000001      // erase chip
#define FICR_ADDRESS 0x10000000
#define FICR_CODEPAGESIZE (FICR_ADDRESS + 0x010)
#define FICR_CODESIZE (FICR_ADDRESS + 0x014)

#define UICR_Addr (0x10001000)
// UICR RBPCONF
#define UICR_RBPCONF (UICR_Addr + 0x0208)
#define RBP_EN 0xFFFFFF00  // appprotect
#define RBP_DS 0xFFFFFFFF  // disable read protect

#define NRF52_FALSH_PAGE_SIZE 4096
#define EEPROM_START 0x00000000
#define EEPROM_START_APP 0x00026000
#endif