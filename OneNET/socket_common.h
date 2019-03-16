#ifndef __SOCKET_COMMON_H__
#define __SOCKET_COMMON_H__



typedef enum
{
	CONNECT_OK = 0,
	CONNECT_ERROR = 1
}socket_connect_t;

typedef enum
{	
	CLOSE_OK = 0,
	CLOSE_ERROR = 1
}socket_close_t;


int socket_create(void);
socket_connect_t socket_connect_service(int socket_id, char *remote_addr, unsigned int remote_port);
int socket_send(int socket_id, unsigned char *data_buf, int data_len);
socket_close_t socket_close(int socket_id);
int socket_receive(int socket_id, char *data_buf, int data_len);






#endif



