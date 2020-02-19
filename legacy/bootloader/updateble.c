//----------------------------------------------------------------------
//      FTSAFE INC COS Kernel : (FIDO)
//----------------------------------------------------------------------
//      File: Update.C
//----------------------------------------------------------------------
//      Update History: (mm/dd/yyyy)
//      06/06/2016 - V1.00 - IQ :  First official release
//----------------------------------------------------------------------
//      Description:
//
//      本模块主要实现通过串口升级BLE
//----------------------------------------------------------------------
#include "swd.h"
#include "updateble.h"


//ucErase:1:erase page ;2:eraseall;0:no erase
unsigned char bUBle_beginUpdateFirmware(void)
{
    unsigned char res;
    
    swd_io_init();
    res = swd_dap_init();  //包含了擦除
    if(res != 1)
    {
        return FALSE;
    }
    g_offset = 0;
    return TRUE;   
}

/*****************************************************************************
 函数:	bUBLE_UpdateBleFirmware
 输入：
 		ulBleLen：ble固件长度
 		ulbleaddr:ble固件的地址
 		ucMode：0是更新；1：生产
 输出：	
 		无
 返回:
 		TRUE/FALSE
******************************************************************************/
unsigned char bUBLE_UpdateBleFirmware(unsigned int ulBleLen,unsigned int ulbleaddr,unsigned char ucMode)
{
	unsigned int templen;
	unsigned char res ;	

	templen = ulBleLen ;
	//首先擦除
	res = bUBle_beginUpdateFirmware();
	if(res == FALSE)
	{
		return FALSE;
	} 
	//然后下载
    while(templen >= g_page_size)
    {
        vHAL_Read(ulbleaddr+g_offset,flashram,(unsigned short)g_page_size);
        res = swd_download(flashram,g_page_size,ucMode);
        if(res != 1)
        {
            return FALSE;
        } 
        g_offset += g_page_size;
        templen -= g_page_size;	

    }
    if( templen )
    {
		memset(flashram,0,g_page_size);
		vHAL_Read(ulbleaddr+g_offset,flashram,(unsigned short)templen);
        res = swd_download(flashram,g_page_size,ucMode);
        if(res != 1)
        {
            return FALSE;
        }
		g_offset += g_page_size;
		templen = 0; 

    }
	//最后检查是否下载成功
    res = swd_check_code(ulbleaddr,g_offset,ucMode);
    if(res != 1)
    {
        return FALSE;
    }	
	return TRUE;

}

