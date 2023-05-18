#ifndef __HTTP_SERVER_H__
#define __HTTP_SERVER_H__
#include "stdint.h"
#include "lwip/opt.h"

#include <lwip/sockets.h>

#include "lwip/sys.h"
#include "lwip/api.h"
void http_init(void);
void onHttpRec(struct netconn *conn, char *met, uint8_t met_size, char *request, size_t request_size, uint8_t *payload, size_t pl_len);
#endif