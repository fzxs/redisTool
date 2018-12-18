
#ifndef __DBT_ERRNO_H_
#define __DBT_ERRNO_H_


#define DEC_OK 0x00000000
#define DEC_FAIL 0x11111111                      /* 默认错误(-1)--暂时redis接口不可用,或者超时等待，获取redis获取数据失败 */
#define DEC_PARAM_FAIL 0x10001110                /* 参数错误(-2) */
#define DEC_CONNECT_FAILED 0x10001101            /* 连接数据库失败(-3) */

#endif

