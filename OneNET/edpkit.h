#ifndef _EDPKIT_H_
#define _EDPKIT_H_

#include "rtthread.h"
#include "stdbool.h"

#define edp_alloc_buffer(buffer_size) rt_malloc(buffer_size);
#define edp_free_buffer(buffer)\
{\
    if (buffer != NULL) {\
        rt_free(buffer);\
        buffer = NULL;\
    }\
}\

//=============================配置==============================
#define MOSQ_MSB(A)         (uint8_t)((A & 0xFF00) >> 8)
#define MOSQ_LSB(A)         (uint8_t)(A & 0x00FF)


/*--------------------------------消息编号--------------------------------*/
#define MSG_ID_HIGH			0x55
#define MSG_ID_LOW			0xAA


/*--------------------------------消息类型--------------------------------*/
/* 连接请求 */
#define CONNREQ             0x10
/* 连接响应 */
#define CONNRESP            0x20
/* 连接关闭 */
#define DISCONNECT			0x40
/* 转发(透传)数据 */
#define PUSHDATA            0x30
/* 存储(转发)数据 */
#define SAVEDATA            0x80
/* 存储确认 */
#define SAVEACK             0x90
/* 命令请求 */
#define CMDREQ              0xA0
/* 命令响应 */
#define CMDRESP             0xB0
/* 心跳请求 */
#define PINGREQ             0xC0
/* 心跳响应 */
#define PINGRESP            0xD0
/* 加密请求 */
#define ENCRYPTREQ          0xE0
/* 加密响应 */
#define ENCRYPTRESP         0xF0


#ifndef NULL
#define NULL (void*)0
#endif


/*--------------------------------SAVEDATA消息支持的格式类型--------------------------------*/
typedef enum
{
	
    kTypeFullJson = 0x01,
	
    kTypeBin = 0x02,
	
    kTypeSimpleJsonWithoutTime = 0x03,
	
    kTypeSimpleJsonWithTime = 0x04,
	
    kTypeString = 0x05
	
} SaveDataType;


/*--------------------------------内存分配方案标志--------------------------------*/
#define MEM_FLAG_NULL		0
#define MEM_FLAG_ALLOC		1
#define MEM_FLAG_STATIC		2


typedef struct Buffer
{
	
	uint8_t	*_data;		//协议数据
	
	uint32_t	_len;		//写入的数据长度
	
	uint32_t	_size;		//缓存总大小
	
	uint8_t	_memFlag;	//内存使用的方案：0-未分配	1-使用的动态分配		2-使用的固定内存
	
} EDP_PACKET_STRUCTURE;


/*--------------------------------删包--------------------------------*/
void EDP_DeleteBuffer(EDP_PACKET_STRUCTURE *edpPacket);

/*--------------------------------解包--------------------------------*/
uint8_t EDP_UnPacketRecv(uint8_t *dataPtr);

/*--------------------------------登录方式1组包--------------------------------*/
bool EDP_PacketConnect1(const char *devid, const char *apikey, uint16_t cTime, EDP_PACKET_STRUCTURE *edpPacket);

/*--------------------------------登录方式2组包--------------------------------*/
bool EDP_PacketConnect2(const char *proid, const char *auth_key, uint16_t cTime, EDP_PACKET_STRUCTURE *edpPacket);

/*--------------------------------连接回复解包--------------------------------*/
uint8_t EDP_UnPacketConnectRsp(uint8_t *rev_data);

/*--------------------------------数据点上传组包--------------------------------*/
uint8_t EDP_PacketSaveData(const char *devid, int16_t send_len, char *type_bin_head, SaveDataType type, EDP_PACKET_STRUCTURE *edpPacket);

/*--------------------------------PushData组包--------------------------------*/
uint8_t EDP_PacketPushData(const char *devid, const char *msg, uint32_t msg_len, EDP_PACKET_STRUCTURE *edpPacket);

/*--------------------------------PushData解包--------------------------------*/
uint8_t EDP_UnPacketPushData(uint8_t *rev_data, char **src_devid, char **req, uint32_t *req_len);

/*--------------------------------命令下发解包--------------------------------*/
uint8_t EDP_UnPacketCmd(uint8_t *rev_data, char **cmdid, uint16_t *cmdid_len, char **req, uint32_t *req_len);

/*--------------------------------命令回复组包--------------------------------*/
bool EDP_PacketCmdResp(const char *cmdid, uint16_t cmdid_len, const char *resp, uint32_t resp_len, EDP_PACKET_STRUCTURE *edpPacket);

/*--------------------------------心跳请求组包--------------------------------*/
bool EDP_PacketPing(EDP_PACKET_STRUCTURE *edpPacket);

#endif
