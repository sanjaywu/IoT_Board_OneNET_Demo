#include "rtthread.h"
#include "sockets.h"
#include "socket_common.h"


/**************************************************************
函数名称:socket_create
函数功能:socket创建
输入参数:无
返 回 值:socket_id
备     注:无
**************************************************************/
int socket_create(void)
{
	int socket_id = -1;
	unsigned char domain = AF_INET;
    unsigned char type = SOCK_STREAM;
    unsigned char protocol = IPPROTO_IP;
	struct timeval send_timeout = {0};
	
	socket_id = socket(domain, type, protocol);

	if(socket_id < 0)
	{
		rt_kprintf("socket create failed!!!, socket_id:%d\r\n", socket_id);
		return socket_id;
	}
	else
	{
		rt_kprintf("socket create success!!!, socket_id:%d\r\n", socket_id);
		send_timeout.tv_sec = 120;
		send_timeout.tv_usec = 0;
		lwip_setsockopt(socket_id, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof(send_timeout));
		return socket_id;
	}
}

/**************************************************************
函数名称:socket_connect_service
函数功能:socket 连接远程服务器
输入参数:socket_id：创建socket时返回的id
		 remote_addr:远程服务器地址
	 	 remote_port:端口
返 回 值:CONNECT_OK：连接成功，CONNECT_ERROR：连接失败
备     注:无
**************************************************************/
socket_connect_t socket_connect_service(int socket_id, char *remote_addr, unsigned int remote_port)
{
	socket_connect_t connect_result = CONNECT_ERROR;
	
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = lwip_htons(remote_port);
	addr.sin_addr.s_addr = inet_addr(remote_addr);
	
	if(0 == connect(socket_id, (struct sockaddr *)&addr, sizeof(addr)))
	{	
		rt_kprintf("socket connect success!!!\r\n");
		connect_result = CONNECT_OK;
	}
	else
	{
		rt_kprintf("socket connect failed!!!\r\n");
		connect_result = CONNECT_ERROR;
	}
	return connect_result;
}

/**************************************************************
函数名称:socket_send
函数功能:socket 发送数据
输入参数:socket_id：创建socket时返回的id
		 data_buf:数据包
		 data_len:数据包大小
返 回 值:发送成功返回数据包大小
备     注	:无
**************************************************************/
int socket_send(int socket_id, unsigned char *data_buf, int data_len)
{
	int send_rec  = 0;

	send_rec = send(socket_id, (char *)data_buf, data_len, 0);

	return send_rec;
}

/**************************************************************
函数名称:socket_close
函数功能:关闭socket
输入参数:socket_id：创建socket时返回的id
返 回 值  	:CLOSE_OK：关闭成功，CLOSE_ERROR：关闭失败
备     注:无
**************************************************************/
socket_close_t socket_close(int socket_id)
{
	socket_close_t close_result = CLOSE_ERROR;

	if(0 == closesocket(socket_id))
	{
		rt_kprintf("socket close success!!!\r\n");
		close_result = CLOSE_OK;
	}
	else
	{
		rt_kprintf("socket close failed!!!\r\n");
		close_result = CLOSE_ERROR;
	}
	return close_result;
}

/**************************************************************
函数名称:socket_receive
函数功能:socket 接收服务器下发的数据
输入参数:socket_id：创建socket时返回的id
	     data_buf：存储接收到的数据区
	     data_len：最大接收大小
返 回 值:成功时返回大于0
备      注:无
**************************************************************/
int socket_receive(int socket_id, char *data_buf, int data_len)
{
	int recv_result = 0;

	recv_result = recv(socket_id, data_buf, data_len, MSG_DONTWAIT);

	return recv_result;
}












