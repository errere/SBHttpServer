#include "http_server.h"


#define HTTP_PORT (80)

#define PRINTF printf

const static unsigned char http_html_hdr[] = "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n";

const unsigned char http_Data[] = "<HTML> \
                                    <head><title>TEST</title><meta charset=\"UTF-8\"></head> \
                                    <center> \
                                    <p> \
                                    <font size=\"6\">test</font> \
                                    </center> \
                                    </HTML> ";


__weak void onHttpRec(struct netconn *conn, char *met, uint8_t met_size, char *request, size_t request_size, uint8_t *payload, size_t pl_len)
{
    PRINTF("HTTP_M = %s\r\n", met);
    PRINTF("HTTP_R = %s\r\n", request);

    PRINTF("HTTP_PL_L = %d\r\n", pl_len);
    for (size_t i = 0; i < pl_len; i++)
    {
        PRINTF("%c", payload[i]);
    }
    PRINTF("===========\r\n\r\n");

    vPortFree(payload);

    if (strcmp(met, "GET") == 0)
    {
        netconn_write(conn, http_html_hdr, strlen(http_html_hdr), NETCONN_NOCOPY);
        netconn_write(conn, http_Data, strlen(http_Data), NETCONN_NOCOPY);
    }
    else if (strcmp(met, "POST") == 0)
    {
        netconn_write(conn, http_html_hdr, strlen(http_html_hdr), NETCONN_NOCOPY);
        netconn_write(conn, http_Data, strlen(http_Data), NETCONN_NOCOPY);
    }
}

static size_t getPayloadEntry(struct netbuf *buf)
{
    netbuf_first(buf);
    uint8_t *data;
    uint16_t len = 0;

    size_t total_len = 0;

    uint8_t status = 0;
    uint8_t compare = '\r';
    do
    {

        netbuf_data(buf, (void **)&data, &len);
        // PRINTF("len = %d\r\n", len);

        size_t i;

        for (i = 0; i < len; ++i)
        {
            if (data[i] == compare)
            {
                switch (status)
                {
                case 0:
                    status = 1;
                    compare = '\n';
                    break;
                case 1:
                    status = 2;
                    compare = '\r';
                    break;
                case 2:
                    status = 3;
                    compare = '\n';
                    break;
                case 3:
                    status = 0;
                    total_len += i;
                    PRINTF("find ok  offset = %d\r\n", total_len);
                    return total_len + 1;
                    break;

                default:
                    status = 0;
                    break;
                }
            }
            else
            {
                status = 0;
            }
        } // for

        total_len += len;

    } while (netbuf_next(buf) >= 0);

    return 0;
}

static uint8_t *getPayload(struct netbuf *buf, size_t *pl_len)
{
    size_t offset = getPayloadEntry(buf);

    netbuf_first(buf);

    uint8_t *data;
    uint16_t len = 0;

    size_t total_len = 0;

    do
    {

        netbuf_data(buf, (void **)&data, &len);

        total_len += len;
        continue;

    } while (netbuf_next(buf) >= 0);

    uint8_t *str = pvPortMalloc(total_len);
    if (str == 0)
    {
        PRINTF("alloc header_str err\r\n");
        goto exit;
    }

    size_t cplen = netbuf_copy(buf, str, total_len);

    if (offset > cplen)
    {
        PRINTF("ERR:offset > cplen\r\n");
        goto exit;
    }
    if (cplen > total_len)
    {
        PRINTF("ERR:cplen > total_len\r\n");
        goto exit;
    }
    if (offset == 0)
    {
        PRINTF("ERR:offset == 0\r\n");
        goto exit;
    }

    size_t data_len = cplen - offset;

    static uint8_t *payload_str;
    payload_str = pvPortMalloc(data_len);

    if (payload_str == 0)
    {
        PRINTF("alloc payload_str err\r\n");
        goto exit;
    }

    for (size_t i = 0; i < data_len; i++)
    {
        payload_str[i] = str[offset + i];
    }

    *pl_len = data_len;

    if (str)
        vPortFree(str);

    return payload_str;
exit:
    if (str)
        vPortFree(str);
    if (payload_str)
        vPortFree(payload_str);
    return NULL;
}

static void onHeader(struct netconn **conn, struct netbuf **buf, size_t len)
{
    uint8_t *header_str = pvPortMalloc(len + 1);
    if (header_str == 0)
    {
        PRINTF("alloc header_str err\r\n");
        goto exit;
    }
    netbuf_copy(*buf, header_str, len);
    header_str[len] = '\0';
    // parse
    PRINTF("HTTP_STR = %s", header_str);

    size_t i, j;
    j = 0;
    for (i = 0; i < len; i++)
    {
        // PRINTF("ch1 = %c",header_str[i]);
        if (header_str[i] == ' ')
        {
            // PRINTF("find space 1 at %d,len = %d\r\n", i, len);
            break;
        }
    }

    uint8_t *met_str, *src_str;
    if (i < len)
    {
        j = i;
        i++;
        for (; i < len; i++)
        {
            // PRINTF("ch2 = %c",header_str[i]);
            if (header_str[i] == ' ')
            {
                // PRINTF("find space 2 at %d,len = %d,j=%d\r\n", i, len, j);
                break;
            }
        }
        if (i < len)
        {
            // success
            met_str = pvPortMalloc(j);
            if (met_str == 0)
            {
                PRINTF("alloc met_str err\r\n");
                goto exit;
            }

            src_str = pvPortMalloc(i - j);
            if (src_str == 0)
            {
                PRINTF("alloc src_str err\r\n");
                goto exit;
            }
            // GET / HTTP/1.1
            //    j i
            // 012345
            memcpy(met_str, header_str, j);
            memcpy(src_str, header_str + j + 1, i - j);
            met_str[j] = 0;
            src_str[i - j - 1] = 0;

            // get payload
            size_t pl_len;
            uint8_t *pl = getPayload(*buf, &pl_len);
            onHttpRec(*conn, met_str, j, src_str, i - j, pl, pl_len);

        } // i < len req

    } // i < len met

exit:
    if (header_str)
        vPortFree(header_str);
    if (met_str)
        vPortFree(met_str);
    if (src_str)
        vPortFree(src_str);
}

static void vTaskHttpThread(void *arg)
{

    struct netconn *conn, *newconn;
    err_t accept_err, recv_err;
    LWIP_UNUSED_ARG(arg);

    /* Create a new connection identifier. */
    /* Bind connection to well known port number 7. */

    conn = netconn_new(NETCONN_TCP);
    netconn_bind(conn, IP_ADDR_ANY, HTTP_PORT);
    LWIP_ERROR("tcpecho: invalid conn", (conn != NULL), return;);

    PRINTF("tcp本地端口号是%d\n\n", HTTP_PORT);

    /* Tell connection to go into listening mode. */
    netconn_listen(conn);

    while (1)
    {

        /* Grab new connection. */
        accept_err = netconn_accept(conn, &newconn);
        PRINTF("accepted new connection %p\n", newconn);
        /* Process the new connection. */
        if (accept_err == ERR_OK)
        {
            struct netbuf *buf;
            // void *data;
            u16_t len;

            uint8_t *http_header;
            uint8_t is_header_down = 0;
            size_t header_size = 0;

            if ((recv_err = netconn_recv(newconn, &buf)) == ERR_OK)
            {
                PRINTF("Recved\n");

                do
                {
                    if (!is_header_down)
                    {

                        netbuf_data(buf, (void **)&http_header, &len);
                        PRINTF("len = %d\r\n", len);
                        // for (size_t i = 0; i < len; i++)
                        // {
                        //     PRINTF("%c", (char)http_header[i]);
                        // }
                        // PRINTF("================\r\n\r\n");
                        size_t i;
                        for (i = 0; i < len; i++)
                        {
                            if (http_header[i] == '\n')
                            {
                                header_size = header_size + i + 1; // myself + '\0'
                                onHeader(&newconn, &buf, header_size);

                                is_header_down = 1;
                                break;
                            }
                        } // for

                        if (i == len)
                        {
                            header_size += len;
                        }
                    } //! is_header_down

                    // err = netconn_write(newconn, data, len, NETCONN_COPY);

                } while (netbuf_next(buf) >= 0);

                PRINTF("buf end\r\n");

            } // netconn_recv

            PRINTF("conn close\r\n");
            /* Close connection and discard connection identifier. */
            if (buf)
            {
                netbuf_delete(buf);
                buf = 0;
            }

            netconn_close(newconn);
            netconn_delete(newconn);
        } // accept_err == ERR_OK
        else
        {
            PRINTF("conn error\r\n");
        }
    } // main loop
}
/*-----------------------------------------------------------------------------------*/
void http_init(void)
{
    sys_thread_new("http", vTaskHttpThread, NULL, 4096, 4);
}
/*-----------------------------------------------------------------------------------*/
