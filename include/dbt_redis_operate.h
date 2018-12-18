
#ifndef __DBT_REDIS_OPERATE_H_
#define __DBT_REDIS_OPERATE_H_

#include "hiredis/hiredis.h"
#include "lock.h"

#include <string>
#include <queue>
#include <vector>
#include <stdint.h>

/*
设计方案
	1.set,get参数说明
	    set函数,get函数支持存储字符串，如果是其他类型需要转成字符串存储
*/

#define TIMEOUT_SPACE 3
#define RECONNECT_LOOP 10                         //重连时间间隔 10s
#define REDIS_ERROR_CODE_OK "OK"

namespace databasetool
{
	typedef enum _EN_DB_STATUS
	{
		DB_STATUS_IDLE = 0x00000000,                       /* 数据库未初始化 */
		DB_STATUS_RUN = 0x00000001,                        /* 数据库正在运行 */
		DB_STATUS_UNUSABLE = 0x00000002,                   /* 数据库不可使用(可能由于无法连接原因，需要重新连接) */
		//DB_STATUS_CONNECTING = 0x00000003,                 /* 正在链接数据库 */  
	}EN_DB_STATUS;

	class TRedisHelper
	{
	public:
		//创建实例对象
		static TRedisHelper *getInstance();

		//重新连接
		static void * reConnect(void *);

	public:
		//初始化
		int init(std::string ip = "127.0.0.1", uint16_t port = 6379, uint32_t nConnect = 1);

		//set
		int set(std::string key, std::string value);

		//setBinary
		int setBinary(std::string key, uint8_t *value, uint32_t vlen);

		//带超时的set
		int setWithTimer(std::string key, std::string value, uint32_t second);

		//带超时的二进制set
		int setBinaryWithTimer(std::string key, uint8_t *value, uint32_t vlen, uint32_t second);

		//get
		int get(std::string key, std::string &value);

		//getBinary
		int getBinary(std::string key, uint8_t *&value, uint32_t &vlen);

	private:
		//重新初始化
		int reInit();

		//连接数据库
		int connectTo(uint32_t n);

		//获取链接
		redisContext * applyContext();

		//释放链接
		void releaseContext(redisContext * context);

		//开始连接
		void startConnect();

	private:
		TRedisHelper() :_ip("127.0.0.1"), _port(6379), _connect_count(1), _state(DB_STATUS_IDLE) {}
		TRedisHelper(TRedisHelper &r);

	private:
		static TRedisHelper * _instance;
		static CMutexLock * _mtx;
		static CCondLock * _cond;

	private:
		std::string _ip;                            /* 数据库IP */
		uint16_t _port;                             /* 数据库端口号 */
		uint32_t _connect_count;                    /* 数据库连接数 */
		EN_DB_STATUS _state;                        /* 数据库状态 */
		std::vector<redisContext *> _contexts;      /* 数据库所有链接 */
		std::queue<redisContext *> _context_queue;  /* 数据库链接队列 */
	};
}

#endif

