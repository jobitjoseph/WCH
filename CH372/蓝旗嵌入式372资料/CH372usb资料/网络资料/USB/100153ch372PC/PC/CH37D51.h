/*
; 单片机内置USB调试固件程序 之 计算机接口程序  V1.0
; 用于连接CH372或者CH375的单片机进行简单的调试功能
; 可以用include直接包含到应用系统的主程序中,或者添加到工程项目中
;
; Website:  http://winchiphead.com
; Email:    tech@winchiphead.com
; @2004.08
;****************************************************************************
*/

/* PC机C语言, VC5.0 */
/* 用于其它类型单片机或者硬件资源不同时, 该程序应该根据需要进行局部修改 */
#ifndef		_CH375_DBG_H
#define		_CH375_DBG_H


//#include "stdafx.h"
#include <windows.h>
#include	"stdlib.h"
#include	"stdio.h"
//#include	<conio.h>
#include "CH375DLL.H"

#ifdef __cplusplus
extern "C" {
#endif

/* **************************************************************************************************** */
// 提供给主程序调用的子程序


// 应用程序启动后,在所有操作之前,必须首先调用一次下面的CH375DBG_GetFirmwareInfo子程序
BOOL	CH375DBG_GetFirmwareInfo( PUCHAR ver );  //获取调试固件程序的版本,并取消未完成的上传数据块
// 输入参数: ver 指向一个字节变量单元,用于存放获取的版本号


// 应用程序退出前,在所有操作之后,应该调用一次下面的CH375DBG_CloseExit子程序
VOID	CH375DBG_CloseExit( VOID );  //关闭CH372/CH375设备


BOOL	CH375DBG_GetAppSysInfo( PUCHAR ver, PUCHAR StringBuffer );  //获取当前应用系统的版本和说明字符串
// 输入参数: ver 指向一个字节变量单元,用于存放获取的版本号
//           StringBuffer 指向一个足够大的字符串缓冲区,用于存放获取的说明字符串


BOOL	CH375DBG_ReadSFR( ULONG addr, PUCHAR data );  //从SFR一个单元读取数据
// 输入参数: addr 指定SFR的地址
//           data 指向一个字节变量单元,用于存放读出的数据


BOOL	CH375DBG_WriteSFR( ULONG addr, UCHAR data );  //向SFR一个单元写入数据
// 输入参数: addr 指定SFR的地址
//           data 指定准备写入的数据


BOOL	CH375DBG_ReadIRAM( ULONG StartAddr, PUCHAR buffer, UCHAR count );  //从内部RAM读取数据块
// 输入参数: StartAddr 指定内部RAM的起始地址
//           buffer 指向一个足够大的数据缓冲区,用于存放读出的数据块
//           count 指定读取的字节数


BOOL	CH375DBG_WriteIRAM( ULONG StartAddr, PUCHAR buffer, UCHAR count );  //向内部RAM写入数据块
// 输入参数: StartAddr 指定内部RAM的起始地址
//           buffer 指向一个数据缓冲区,存放准备写入的数据块
//           count 指定写入的字节数


BOOL	CH375DBG_ReadXRAM( ULONG StartAddr, PUCHAR buffer, UCHAR count );  //从外部RAM读取数据块
// 输入参数: StartAddr 指定外部RAM的起始地址
//           buffer 指向一个足够大的数据缓冲区,用于存放读出的数据块
//           count 指定读取的字节数


BOOL	CH375DBG_WriteXRAM( ULONG StartAddr, PUCHAR buffer, UCHAR count );  //向外部RAM写入数据块
// 输入参数: StartAddr 指定外部RAM的起始地址
//           buffer 指向一个数据缓冲区,存放准备写入的数据块
//           count 指定写入的字节数


BOOL	CH375DBG_ReadROM( ULONG StartAddr, PUCHAR buffer, UCHAR count );  //从程序ROM读取数据块
// 输入参数: StartAddr 指定程序ROM的起始地址
//           buffer 指向一个足够大的数据缓冲区,用于存放读出的数据块
//           count 指定读取的字节数


UCHAR	CH375DBG_GetPortP1( VOID );  //获取MCS51单片机端口P1的状态
// 输出结果: 返回当前端口状态值


VOID	CH375DBG_PutPortP1( UCHAR d );  //设置MCS51单片机端口P1的状态
// 输入参数: d 指定新的端口状态值


/* **************************************************************************************************** */
// 计算机接口子程序源程序



typedef unsigned char           UINT8;
typedef unsigned short          UINT16;
typedef unsigned long           UINT32;

#ifndef MAX_DATA_SIZE
#define MAX_DATA_SIZE		16			/* 单次命令处理的最大数据长度,有效值是1到56 */
#endif

#pragma pack( push, old_pack, 1 )

typedef	struct	_USB_DOWN_PACKET {		/* 下传的数据包结构,用于命令/写数据 */
	UINT8	mCommand;					/* 命令码,见下面的定义 */
	UINT8	mCommandNot;				/* 命令码的反码,用于校验下传数据包 */
	union {
		UINT8	mByte[4];				/* 通用参数 */
		UINT16	mWord[2];				/* 通用参数,低字节在前,Little-Endian */
		UINT32	mDword;					/* 通用参数,低字节在前,Little-Endian */
		void	*mAddress;				/* 读写操作的起始地址,低字节在前,Little-Endian */
	} u;
	UINT8	mLength;					/* 下面的缓冲区的长度,读写操作的字节数 */
	UINT8	mBuffer[ MAX_DATA_SIZE ];	/* 数据缓冲区 */
}	USB_DOWN_PKT;

typedef	struct	_USB_UP_PACKET {		/* 上传的数据包结构,用于状态/读数据 */
	UINT8	mStatus;					/* 状态码,见下面的定义 */
	UINT8	mCommandNot;				/* 命令码的反码,用于校验上传数据包 */
	UINT8	mReserved[4];
	UINT8	mLength;					/* 下面的缓冲区的长度,读操作的字节数 */
	UINT8	mBuffer[ MAX_DATA_SIZE ];	/* 数据缓冲区 */
}	USB_UP_PKT;

#pragma pack( pop, old_pack )

/* 命令码定义,按位说明
   位7为命令类型:  0=实现特定功能, 1=存储器和SFR读写
   对于"实现特定功能"命令类型:
       位6-位0为定义的具体命令码, 命令码为00H-7FH, 其中: 00H-3FH为通用标准命令, 40H-7FH为与应用系统有关的特定命令
       目前版本定义了以下通用标准命令:
           00H: 获取调试固件程序的版本,并取消未完成的上传数据块
           10H: 获取当前应用系统的版本和说明字符串
   对于"存储器和SFR读写"命令类型:
       位6为数据传输方向:      0=读操作/上传, 1=写操作/下传
       位5-位4为数据读写宽度:  00=以字节为单位/8位, 01=以字为单位/16位, 10=以双字为单位/32位, 11=以位为单位/1位
       位1-位0为存储器空间:    00=存取SFR, 01=存取内部RAM, 10=存取外部RAM, 11=存取程序ROM
       例如: 命令码80H为读SFR, 命令码83H为读程序ROM, 命令码C1H为写内部RAM, 命令码C2H为写外部RAM
   状态码定义: 00H为操作成功, 080H为命令不支持, 0FFH为未定义的错误 */

#define USB_CMD_GET_FW_INFO		0x00
#define USB_CMD_GET_APP_INFO	0x10

#define USB_CMD_MEM_ACCESS		0x80
#define USB_CMD_MEM_DIR_WR		0x40
#define USB_CMD_MEM_WIDTH		0x0C
#define USB_CMD_MEM_W_BYTE		0x00
#define USB_CMD_MEM_W_WORD		0x04
#define USB_CMD_MEM_W_DWORD		0x08
#define USB_CMD_MEM_W_BIT		0x0C
#define USB_CMD_MEM_SPACE		0x03
#define USB_CMD_MEM_S_SFR		0x00
#define USB_CMD_MEM_S_IRAM		0x01
#define USB_CMD_MEM_S_XRAM		0x02
#define USB_CMD_MEM_S_ROM		0x03

#define ERR_SUCCESS				0x00
#define ERR_UNSUPPORT			0x80
#define ERR_UNDEFINED			0xFF


/* **************************************************************************************************** */

#ifdef __cplusplus
}
#endif
#endif	// _CH375_DBG_H
