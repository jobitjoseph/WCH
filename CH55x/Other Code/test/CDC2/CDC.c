/********************************** (C) COPYRIGHT *******************************
* File Name          : CDC.C
* Author             : WCH
* Version            : V1.0
* Date               : 2017/03/01
* Description        : CH554做CDC设备转串口，选择串口1
*******************************************************************************/
#include "CH554.H"
#include "DEBUG.H"
#include <stdio.h>
#include <string.h>

#include "gpio.h"
#include "touchkey.h"
     
#pragma  NOAREGS
		 
UINT8X  Ep0Buffer[DEFAULT_ENDP0_SIZE] _at_ 0x0000;                                 //端点0 OUT&IN缓冲区，必须是偶地址
UINT8X  Ep1Buffer[8] _at_ 0x0040;                                                  //端点1上传缓冲区
UINT8X  Ep2Buffer[2*MAX_PACKET_SIZE] _at_ 0x0080;                                  //端点2 IN & OUT缓冲区,必须是偶地址

UINT16 SetupLen;
UINT8   SetupReq,Count,UsbConfig;
PUINT8  pDescr;                                                                //USB配置标志
USB_SETUP_REQ   SetupReqBuf;                                                   //暂存Setup包
#define UsbSetupBuf     ((PUSB_SETUP_REQ)Ep0Buffer)

#define  SET_LINE_CODING                0X20            // Configures DTE rate, stop-bits, parity, and number-of-character
#define  GET_LINE_CODING                0X21            // This request allows the host to find out the currently configured line coding.
#define  SET_CONTROL_LINE_STATE         0X22            // This request generates RS-232/V.24 style control signals.


/*设备描述符*/
UINT8C DevDesc[] = {0x12,0x01,0x10,0x01,0x02,0x00,0x00,DEFAULT_ENDP0_SIZE,
                    0x86,0x1a,0x22,0x57,0x00,0x01,0x01,0x02,
                    0x03,0x01
                   };
UINT8C CfgDesc[] ={
    0x09,0x02,0x43,0x00,0x02,0x01,0x00,0xa0,0x32,             //配置描述符（两个接口）
	//以下为接口0（CDC接口）描述符	
    0x09,0x04,0x00,0x00,0x01,0x02,0x02,0x01,0x00,             //CDC接口描述符(一个端点)
	//以下为功能描述符
    0x05,0x24,0x00,0x10,0x01,                                 //功能描述符(头)
	0x05,0x24,0x01,0x00,0x00,                                 //管理描述符(没有数据类接口) 03 01
	0x04,0x24,0x02,0x02,                                      //支持Set_Line_Coding、Set_Control_Line_State、Get_Line_Coding、Serial_State	
	0x05,0x24,0x06,0x00,0x01,                                 //编号为0的CDC接口;编号1的数据类接口
	0x07,0x05,0x81,0x03,0x08,0x00,0xFF,                       //中断上传端点描述符
	//以下为接口1（数据接口）描述符
	0x09,0x04,0x01,0x00,0x02,0x0a,0x00,0x00,0x00,             //数据接口描述符
    0x07,0x05,0x02,0x02,0x40,0x00,0x00,                       //端点描述符	
	0x07,0x05,0x82,0x02,0x40,0x00,0x00,                       //端点描述符
};
/*字符串描述符*/
 unsigned char  code LangDes[]={0x04,0x03,0x09,0x04};           //语言描述符
 unsigned char  code SerDes[]={                                 //序列号字符串描述符
                0x14,0x03,
				0x32,0x00,0x30,0x00,0x31,0x00,0x37,0x00,0x2D,0x00,
				0x32,0x00,0x2D,0x00,
				0x32,0x00,0x35,0x00
                };     
 unsigned char  code Prod_Des[]={                                //产品字符串描述符
				0x14,0x03,
				0x43,0x00,0x48,0x00,0x35,0x00,0x35,0x00,0x34,0x00,0x5F,0x00,
				0x43,0x00,0x44,0x00,0x43,0x00,
 };
 unsigned char  code Manuf_Des[]={  
				0x0A,0x03,
				0x5F,0x6c,0xCF,0x82,0x81,0x6c,0x52,0x60,
 };

//cdc参数
UINT8X LineCoding[7]={0x00,0xe1,0x00,0x00,0x00,0x00,0x08};   //初始化波特率为57600，1停止位，无校验，8数据位。

#define UART_REV_LEN  64                 //串口接收缓冲区大小
UINT8I Receive_Uart_Buf[UART_REV_LEN];   //串口接收缓冲区
volatile UINT8I Uart_Input_Point = 0;   //循环缓冲区写入指针，总线复位需要初始化为0
volatile UINT8I Uart_Output_Point = 0;  //循环缓冲区取出指针，总线复位需要初始化为0
volatile UINT8I UartByteCount = 0;      //当前缓冲区剩余待取字节数


volatile UINT8I USBByteCount = 0;      //代表USB端点接收到的数据
volatile UINT8I USBBufOutPoint = 0;    //取数据指针

volatile UINT8I UpPoint2_Busy  = 0;   //上传端点是否忙标志


/*******************************************************************************
* Function Name  : USBDeviceCfg()
* Description    : USB设备模式配置
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void USBDeviceCfg()
{
    USB_CTRL = 0x00;                                                           	//清空USB控制寄存器
    USB_CTRL &= ~bUC_HOST_MODE;                                                	//该位为选择设备模式
    USB_CTRL |=  bUC_DEV_PU_EN | bUC_INT_BUSY | bUC_DMA_EN;                    	//USB设备和内部上拉使能,在中断期间中断标志未清除前自动返回NAK
    USB_DEV_AD = 0x00;                                                         	//设备地址初始化
//     USB_CTRL |= bUC_LOW_SPEED;
//     UDEV_CTRL |= bUD_LOW_SPEED;                                              //选择低速1.5M模式
    USB_CTRL &= ~bUC_LOW_SPEED;
    UDEV_CTRL &= ~bUD_LOW_SPEED;                                             		//选择全速12M模式，默认方式
	  UDEV_CTRL = bUD_PD_DIS;  																										// 禁止DP/DM下拉电阻
    UDEV_CTRL |= bUD_PORT_EN;                                                  	//使能物理端口
}
/*******************************************************************************
* Function Name  : USBDeviceIntCfg()
* Description    : USB设备模式中断初始化
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void USBDeviceIntCfg()
{
    USB_INT_EN |= bUIE_SUSPEND;                                               //使能设备挂起中断
    USB_INT_EN |= bUIE_TRANSFER;                                              //使能USB传输完成中断
    USB_INT_EN |= bUIE_BUS_RST;                                               //使能设备模式USB总线复位中断
    USB_INT_FG |= 0x1F;                                                       //清中断标志
    IE_USB = 1;                                                               //使能USB中断
    EA = 1;                                                                   //允许单片机中断
}
/*******************************************************************************
* Function Name  : USBDeviceEndPointCfg()
* Description    : USB设备模式端点配置，模拟兼容HID设备，除了端点0的控制传输，还包括端点2批量上下传
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void USBDeviceEndPointCfg()
{
	UEP1_DMA = Ep1Buffer;                                                      //端点1 发送数据传输地址
    UEP2_DMA = Ep2Buffer;                                                      //端点2 IN数据传输地址	
    UEP2_3_MOD = 0xCC;                                                         //端点2/3 单缓冲收发使能
    UEP2_CTRL = bUEP_AUTO_TOG | UEP_T_RES_NAK | UEP_R_RES_ACK;                 //端点2自动翻转同步标志位，IN事务返回NAK，OUT返回ACK

    UEP1_CTRL = bUEP_AUTO_TOG | UEP_T_RES_NAK;                                 //端点1自动翻转同步标志位，IN事务返回NAK	
	UEP0_DMA = Ep0Buffer;                                                      //端点0数据传输地址
    UEP4_1_MOD = 0X40;                                                         //端点1上传缓冲区；端点0单64字节收发缓冲区
    UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;                                 //手动翻转，OUT事务返回ACK，IN事务返回NAK
}
/*******************************************************************************
* Function Name  : Config_Uart1(UINT8 *cfg_uart)
* Description    : 配置串口1参数
* Input          : 串口配置参数 四位波特率、停止位、校验、数据位
* Output         : None
* Return         : None
*******************************************************************************/
void Config_Uart1(UINT8 *cfg_uart)
{
	UINT32 uart1_buad = 0;
	*((UINT8 *)&uart1_buad) = cfg_uart[3];
	*((UINT8 *)&uart1_buad+1) = cfg_uart[2];
	*((UINT8 *)&uart1_buad+2) = cfg_uart[1];
	*((UINT8 *)&uart1_buad+3) = cfg_uart[0];
	IE_UART1 = 0;
	SBAUD1 = 0 - FREQ_SYS/16/uart1_buad;
	IE_UART1 = 1;
}
/*******************************************************************************
* Function Name  : DeviceInterrupt()
* Description    : CH559USB中断处理函数
*******************************************************************************/
void    DeviceInterrupt( void ) interrupt INT_NO_USB                       //USB中断服务程序,使用寄存器组1
{
    UINT16 len;
    if(UIF_TRANSFER)                                                            //USB传输完成标志
    {
        switch (USB_INT_ST & (MASK_UIS_TOKEN | MASK_UIS_ENDP))
        {
		case UIS_TOKEN_IN | 1:                                                  //endpoint 1# 端点中断上传
			UEP1_T_LEN = 0; 
			UEP1_CTRL = UEP1_CTRL & ~ MASK_UEP_T_RES | UEP_T_RES_NAK;           //默认应答NAK
			break;
        case UIS_TOKEN_IN | 2:                                                  //endpoint 2# 端点批量上传
			{		
				UEP2_T_LEN = 0;                                                    //预使用发送长度一定要清空
				UEP2_CTRL = UEP2_CTRL & ~ MASK_UEP_T_RES | UEP_T_RES_NAK;           //默认应答NAK
				UpPoint2_Busy = 0;                                                  //清除忙标志
			}
            break;
        case UIS_TOKEN_OUT | 2:                                                 //endpoint 3# 端点批量下传
            if ( U_TOG_OK )                                                     // 不同步的数据包将丢弃
            {
                USBByteCount = USB_RX_LEN;
				USBBufOutPoint = 0;                                             //取数据指针复位
				UEP2_CTRL = UEP2_CTRL & ~ MASK_UEP_R_RES | UEP_R_RES_NAK;       //收到一包数据就NAK，主函数处理完，由主函数修改响应方式
            }
            break;
        case UIS_TOKEN_SETUP | 0:                                                //SETUP事务
            len = USB_RX_LEN;
            if(len == (sizeof(USB_SETUP_REQ)))
            {
                SetupLen = ((UINT16)UsbSetupBuf->wLengthH<<8) | (UsbSetupBuf->wLengthL);
                len = 0;                                                      // 默认为成功并且上传0长度
                SetupReq = UsbSetupBuf->bRequest;							
                if ( ( UsbSetupBuf->bRequestType & USB_REQ_TYP_MASK ) != USB_REQ_TYP_STANDARD )//非标准请求
                {
									switch( SetupReq ) 
									{
										case GET_LINE_CODING:   //0x21  currently configured
											pDescr = LineCoding;
											len = sizeof(LineCoding);
											len = SetupLen >= DEFAULT_ENDP0_SIZE ? DEFAULT_ENDP0_SIZE : SetupLen;  // 本次传输长度
											memcpy(Ep0Buffer,pDescr,len); 
											SetupLen -= len;
											pDescr += len;
											break;						
										case SET_CONTROL_LINE_STATE:  //0x22  generates RS-232/V.24 style control signals										
											break;
										case SET_LINE_CODING:      //0x20  Configure
											break;
										default:
												 len = 0xFF;  								 					                 /*命令不支持*/					
												 break;
									}		
                }
                else                                                             //标准请求
                {
                    switch(SetupReq)                                             //请求码
                    {
                    case USB_GET_DESCRIPTOR:
                        switch(UsbSetupBuf->wValueH)
                        {
                        case 1:                                                       //设备描述符
                            pDescr = DevDesc;                                         //把设备描述符送到要发送的缓冲区
                            len = sizeof(DevDesc);
                            break;
                        case 2:                                                        //配置描述符
                            pDescr = CfgDesc;                                          //把设备描述符送到要发送的缓冲区
                            len = sizeof(CfgDesc);
                            break;
												case 3:
													if(UsbSetupBuf->wValueL == 0)
													{
														pDescr = LangDes;                                          
														len = sizeof(LangDes);								
													}
													else if(UsbSetupBuf->wValueL == 1)
													{
														pDescr = Manuf_Des; 
														len = sizeof(Manuf_Des);
													}
													else if(UsbSetupBuf->wValueL == 2)
													{
														pDescr = Prod_Des; 
														len = sizeof(Prod_Des);
													}
													else
													{
														pDescr = SerDes; 
														len = sizeof(SerDes);
													}							
													break;
                        default:
                            len = 0xff;                                                //不支持的命令或者出错
                            break;
                        }
                        if ( SetupLen > len )
                        {
                            SetupLen = len;    //限制总长度
                        }
                        len = SetupLen >= DEFAULT_ENDP0_SIZE ? DEFAULT_ENDP0_SIZE : SetupLen;                            //本次传输长度
                        memcpy(Ep0Buffer,pDescr,len);                                  //加载上传数据
                        SetupLen -= len;
                        pDescr += len;
                        break;
                    case USB_SET_ADDRESS:
                        SetupLen = UsbSetupBuf->wValueL;                              //暂存USB设备地址
                        break;
                    case USB_GET_CONFIGURATION:
                        Ep0Buffer[0] = UsbConfig;
                        if ( SetupLen >= 1 )
                        {
                            len = 1;
                        }
                        break;
                    case USB_SET_CONFIGURATION:
                        UsbConfig = UsbSetupBuf->wValueL;
                        break;
                    case USB_GET_INTERFACE:
                        break;
                    case USB_CLEAR_FEATURE:                                            //Clear Feature
                        if( ( UsbSetupBuf->bRequestType & 0x1F ) == USB_REQ_RECIP_DEVICE )                  /* 清除设备 */
                        {
                            if( ( ( ( UINT16 )UsbSetupBuf->wValueH << 8 ) | UsbSetupBuf->wValueL ) == 0x01 )
                            {
                                if( CfgDesc[ 7 ] & 0x20 )
                                {
                                    /* 唤醒 */
                                }
                                else
                                {
                                    len = 0xFF;                                        /* 操作失败 */
                                }
                            }
                            else
                            {
                                len = 0xFF;                                            /* 操作失败 */
                            }
                        }
                        else if ( ( UsbSetupBuf->bRequestType & USB_REQ_RECIP_MASK ) == USB_REQ_RECIP_ENDP )// 端点
                        {
                            switch( UsbSetupBuf->wIndexL )
                            {
                            case 0x83:
                                UEP3_CTRL = UEP3_CTRL & ~ ( bUEP_T_TOG | MASK_UEP_T_RES ) | UEP_T_RES_NAK;
                                break;
                            case 0x03:
                                UEP3_CTRL = UEP3_CTRL & ~ ( bUEP_R_TOG | MASK_UEP_R_RES ) | UEP_R_RES_ACK;
                                break;
                            case 0x82:
                                UEP2_CTRL = UEP2_CTRL & ~ ( bUEP_T_TOG | MASK_UEP_T_RES ) | UEP_T_RES_NAK;
                                break;
                            case 0x02:
                                UEP2_CTRL = UEP2_CTRL & ~ ( bUEP_R_TOG | MASK_UEP_R_RES ) | UEP_R_RES_ACK;
                                break;
                            case 0x81:
                                UEP1_CTRL = UEP1_CTRL & ~ ( bUEP_T_TOG | MASK_UEP_T_RES ) | UEP_T_RES_NAK;
                                break;
                            case 0x01:
                                UEP1_CTRL = UEP1_CTRL & ~ ( bUEP_R_TOG | MASK_UEP_R_RES ) | UEP_R_RES_ACK;
                                break;							
                            default:
                                len = 0xFF;                                         // 不支持的端点
                                break;
                            }
                        }
                        else
                        {
                            len = 0xFF;                                                // 不是端点不支持
                        }
                        break;
                    case USB_SET_FEATURE:                                          /* Set Feature */
                        if( ( UsbSetupBuf->bRequestType & 0x1F ) == USB_REQ_RECIP_DEVICE )                  /* 设置设备 */
                        {
                            if( ( ( ( UINT16 )UsbSetupBuf->wValueH << 8 ) | UsbSetupBuf->wValueL ) == 0x01 )
                            {
                                if( CfgDesc[ 7 ] & 0x20 )
                                {
                                    /* 休眠 */
																	#ifdef DE_PRINTF
																				printf( "suspend\n" );                                                             //睡眠状态
																	#endif
																	while ( XBUS_AUX & bUART0_TX )
																	{
																		;    //等待发送完成
																	}
																	SAFE_MOD = 0x55;
																	SAFE_MOD = 0xAA;
																	WAKE_CTRL = bWAK_BY_USB | bWAK_RXD0_LO | bWAK_RXD1_LO;                      //USB或者RXD0/1有信号时可被唤醒
																	PCON |= PD;                                                                 //睡眠
																	SAFE_MOD = 0x55;
																	SAFE_MOD = 0xAA;
																	WAKE_CTRL = 0x00;
                                }
                                else
                                {
                                    len = 0xFF;                                        /* 操作失败 */
                                }
                            }
                            else
                            {
                                len = 0xFF;                                            /* 操作失败 */
                            }
                        }
                        else if( ( UsbSetupBuf->bRequestType & 0x1F ) == USB_REQ_RECIP_ENDP )             /* 设置端点 */
                        {
                            if( ( ( ( UINT16 )UsbSetupBuf->wValueH << 8 ) | UsbSetupBuf->wValueL ) == 0x00 )
                            {
                                switch( ( ( UINT16 )UsbSetupBuf->wIndexH << 8 ) | UsbSetupBuf->wIndexL )
                                {
                                case 0x83:
                                    UEP3_CTRL = UEP3_CTRL & (~bUEP_T_TOG) | UEP_T_RES_STALL;/* 设置端点3 IN STALL */
                                    break;
                                case 0x03:
                                    UEP3_CTRL = UEP3_CTRL & (~bUEP_R_TOG) | UEP_R_RES_STALL;/* 设置端点3 OUT Stall */
                                    break;									
                                case 0x82:
                                    UEP2_CTRL = UEP2_CTRL & (~bUEP_T_TOG) | UEP_T_RES_STALL;/* 设置端点2 IN STALL */
                                    break;
                                case 0x02:
                                    UEP2_CTRL = UEP2_CTRL & (~bUEP_R_TOG) | UEP_R_RES_STALL;/* 设置端点2 OUT Stall */
                                    break;
                                case 0x81:
                                    UEP1_CTRL = UEP1_CTRL & (~bUEP_T_TOG) | UEP_T_RES_STALL;/* 设置端点1 IN STALL */
                                    break;
								case 0x01:
									UEP1_CTRL = UEP1_CTRL & (~bUEP_R_TOG) | UEP_R_RES_STALL;/* 设置端点1 OUT Stall */
                                default:
                                    len = 0xFF;                                    /* 操作失败 */
                                    break;
                                }
                            }
                            else
                            {
                                len = 0xFF;                                      /* 操作失败 */
                            }
                        }
                        else
                        {
                            len = 0xFF;                                          /* 操作失败 */
                        }
                        break;
                    case USB_GET_STATUS:
                        Ep0Buffer[0] = 0x00;
                        Ep0Buffer[1] = 0x00;
                        if ( SetupLen >= 2 )
                        {
                            len = 2;
                        }
                        else
                        {
                            len = SetupLen;
                        }
                        break;
                    default:
                        len = 0xff;                                                    //操作失败
                        break;
                    }
                }
            }
            else
            {
                len = 0xff;                                                         //包长度错误
            }
            if(len == 0xff)
            {
                SetupReq = 0xFF;
                UEP0_CTRL = bUEP_R_TOG | bUEP_T_TOG | UEP_R_RES_STALL | UEP_T_RES_STALL;//STALL
            }
            else if(len <= DEFAULT_ENDP0_SIZE)                                                       //上传数据或者状态阶段返回0长度包
            {
                UEP0_T_LEN = len;
                UEP0_CTRL = bUEP_R_TOG | bUEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_ACK;//默认数据包是DATA1，返回应答ACK
            }
            else
            {
                UEP0_T_LEN = 0;  //虽然尚未到状态阶段，但是提前预置上传0长度数据包以防主机提前进入状态阶段
                UEP0_CTRL = bUEP_R_TOG | bUEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_ACK;//默认数据包是DATA1,返回应答ACK
            }
            break;
        case UIS_TOKEN_IN | 0:                                                      //endpoint0 IN
            switch(SetupReq)
            {
            case USB_GET_DESCRIPTOR:
                len = SetupLen >= DEFAULT_ENDP0_SIZE ? DEFAULT_ENDP0_SIZE : SetupLen;                                 //本次传输长度
                memcpy( Ep0Buffer, pDescr, len );                                   //加载上传数据
                SetupLen -= len;
                pDescr += len;
                UEP0_T_LEN = len;
                UEP0_CTRL ^= bUEP_T_TOG;                                             //同步标志位翻转
                break;
            case USB_SET_ADDRESS:
                USB_DEV_AD = USB_DEV_AD & bUDA_GP_BIT | SetupLen;
                UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
                break;
            default:
                UEP0_T_LEN = 0;                                                      //状态阶段完成中断或者是强制上传0长度数据包结束控制传输
                UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
                break;
            }
            break;
        case UIS_TOKEN_OUT | 0:  // endpoint0 OUT
			if(SetupReq ==SET_LINE_CODING)  //设置串口属性
			{
				if( U_TOG_OK ) 
				{
					memcpy(LineCoding,UsbSetupBuf,USB_RX_LEN);
//					Config_Uart1(LineCoding);
					UEP0_T_LEN = 0;
					UEP0_CTRL |= UEP_R_RES_ACK | UEP_T_RES_ACK;  // 准备上传0包				
				}
			}
			else
			{
				UEP0_T_LEN = 0;  
				UEP0_CTRL |= UEP_R_RES_ACK | UEP_T_RES_NAK;  //状态阶段，对IN响应NAK
			}
            break;

					
					
        default:
            break;
        }
        UIF_TRANSFER = 0;                                                           //写0清空中断
    }
    if(UIF_BUS_RST)                                                                 //设备模式USB总线复位中断
    {
#ifdef DE_PRINTF
            printf( "reset\n" );                                                             //睡眠状态
#endif		
        UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
        UEP1_CTRL = bUEP_AUTO_TOG | UEP_T_RES_NAK;
        UEP2_CTRL = bUEP_AUTO_TOG | UEP_T_RES_NAK | UEP_R_RES_ACK;
        USB_DEV_AD = 0x00;
        UIF_SUSPEND = 0;
        UIF_TRANSFER = 0;
        UIF_BUS_RST = 0;                                                             //清中断标志
				Uart_Input_Point = 0;   //循环缓冲区输入指针
				Uart_Output_Point = 0;  //循环缓冲区读出指针
				UartByteCount = 0;      //当前缓冲区剩余待取字节数
				USBByteCount = 0;       //USB端点收到的长度
				UsbConfig = 0;          //清除配置值
				UpPoint2_Busy = 0;
    }
    if (UIF_SUSPEND)                                                                 //USB总线挂起/唤醒完成
    {
        UIF_SUSPEND = 0;
        if ( USB_MIS_ST & bUMS_SUSPEND )                                             //挂起
        {
#ifdef DE_PRINTF
            printf( "suspend\n" );                                                             //睡眠状态
#endif
            while ( XBUS_AUX & bUART0_TX )
            {
                ;    //等待发送完成
            }
            SAFE_MOD = 0x55;
            SAFE_MOD = 0xAA;
            WAKE_CTRL = bWAK_BY_USB | bWAK_RXD0_LO | bWAK_RXD1_LO;                      //USB或者RXD0/1有信号时可被唤醒
            PCON |= PD;                                                                 //睡眠
            SAFE_MOD = 0x55;
            SAFE_MOD = 0xAA;
            WAKE_CTRL = 0x00;
        }
    }
    else {                                                                             //意外的中断,不可能发生的情况
        USB_INT_FG = 0xFF;                                                             //清中断标志

    }
}
/*******************************************************************************
* Function Name  : Uart1_ISR()
* Description    : 串口接收中断函数，实现循环缓冲接收
*******************************************************************************/
void Uart1_ISR(void) interrupt INT_NO_UART1
{
	if(U1RI)   //收到数据
	{
		Receive_Uart_Buf[Uart_Input_Point++] = SBUF1;
		UartByteCount++;                    //当前缓冲区剩余待取字节数
		if(Uart_Input_Point>=UART_REV_LEN)
			Uart_Input_Point = 0;           //写入指针
		U1RI =0;		
	}
	
}

void Fill_Buffer(UINT8 dat)
{
	Receive_Uart_Buf[Uart_Input_Point++] = dat;
	UartByteCount++;                    //当前缓冲区剩余待取字节数
	if(Uart_Input_Point>=UART_REV_LEN)
		Uart_Input_Point = 0;           //写入指针
}

sbit LED0 = P1^6;
sbit LED1 = P1^7;

//主函数
main()
{
		UINT8 lenth;
		UINT8 Uart_Timeout = 0;
		UINT8	key = 0;
    CfgFsys( );                                                           //CH559时钟选择配置
    mDelaymS(5);                                                          //修改主频等待内部晶振稳定,必加	
    mInitSTDIO( );                                                        //串口0,可以用于调试
//		UART1Setup( );                                                        //用于CDC

		TK_Init( BIT4+BIT5+BIT6+BIT7,  1, 1 );		/* Choose TIN2, TIN3, TIN4, TIN5 for touchkey input. enable interrupt. */
		TK_SelectChannel(0);											/* NOTICE: ch is not compatible with the IO actually. */

//		Port1Cfg(1,6);                                                             //P16设置推挽模式
//    Port1Cfg(1,7);                                                             //P17设置推挽模式
//    LED0 = 0;
//    LED1 = 0;	
	
#ifdef DE_PRINTF
    printf("start ...\n");
#endif	
    USBDeviceCfg();                                                    
    USBDeviceEndPointCfg();                                               //端点配置
    USBDeviceIntCfg();                                                    //中断初始化
		UEP0_T_LEN = 0;
    UEP1_T_LEN = 0;                                                       //预使用发送长度一定要清空
    UEP2_T_LEN = 0;                                                       //预使用发送长度一定要清空
	
    while(1)
    {
			if( Touch_IN != 0 )
			{
				key = Touch_IN;
				Touch_IN = 0;
				Fill_Buffer(key);
			}
				if(UsbConfig)
				{
					if(USBByteCount)	//usb 接收端有数据
					{
						USBByteCount = 0;
						UEP2_CTRL = UEP2_CTRL & ~ MASK_UEP_R_RES | UEP_R_RES_ACK;	//
					}
					if(UartByteCount)
						Uart_Timeout++;
					if(!UpPoint2_Busy)   //端点不繁忙（空闲后的第一包数据，只用作触发上传）
					{
							if(UartByteCount)
							{
								lenth = UartByteCount;
								if(lenth>0)
								{
									if(lenth>39 || Uart_Timeout>100)
									{			
										Uart_Timeout = 0;
										if(Uart_Output_Point+lenth>UART_REV_LEN)
											lenth = UART_REV_LEN-Uart_Output_Point;
										UartByteCount -= lenth;			
										//写上传端点
										memcpy(Ep2Buffer+MAX_PACKET_SIZE,&Receive_Uart_Buf[Uart_Output_Point],lenth);
										Uart_Output_Point+=lenth;
										if(Uart_Output_Point>=UART_REV_LEN)
											Uart_Output_Point = 0;						
										UEP2_T_LEN = lenth;                                                    //预使用发送长度一定要清空
										UEP2_CTRL = UEP2_CTRL & ~ MASK_UEP_T_RES | UEP_T_RES_ACK;            //应答ACK
										UpPoint2_Busy = 1;
									}
								}
							}
					}
				}		
		}
}
