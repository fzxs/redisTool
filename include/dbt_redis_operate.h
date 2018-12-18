
#ifndef __DBT_REDIS_OPERATE_H_
#define __DBT_REDIS_OPERATE_H_

#include "hiredis/hiredis.h"
#include "lock.h"

#include <string>
#include <queue>
#include <vector>
#include <stdint.h>

/*
��Ʒ���
	1.set,get����˵��
	    set����,get����֧�ִ洢�ַ��������������������Ҫת���ַ����洢
*/

#define TIMEOUT_SPACE 3
#define RECONNECT_LOOP 10                         //����ʱ���� 10s
#define REDIS_ERROR_CODE_OK "OK"

namespace databasetool
{
	typedef enum _EN_DB_STATUS
	{
		DB_STATUS_IDLE = 0x00000000,                       /* ���ݿ�δ��ʼ�� */
		DB_STATUS_RUN = 0x00000001,                        /* ���ݿ��������� */
		DB_STATUS_UNUSABLE = 0x00000002,                   /* ���ݿⲻ��ʹ��(���������޷�����ԭ����Ҫ��������) */
		//DB_STATUS_CONNECTING = 0x00000003,                 /* �����������ݿ� */  
	}EN_DB_STATUS;

	class TRedisHelper
	{
	public:
		//����ʵ������
		static TRedisHelper *getInstance();

		//��������
		static void * reConnect(void *);

	public:
		//��ʼ��
		int init(std::string ip = "127.0.0.1", uint16_t port = 6379, uint32_t nConnect = 1);

		//set
		int set(std::string key, std::string value);

		//setBinary
		int setBinary(std::string key, uint8_t *value, uint32_t vlen);

		//����ʱ��set
		int setWithTimer(std::string key, std::string value, uint32_t second);

		//����ʱ�Ķ�����set
		int setBinaryWithTimer(std::string key, uint8_t *value, uint32_t vlen, uint32_t second);

		//get
		int get(std::string key, std::string &value);

		//getBinary
		int getBinary(std::string key, uint8_t *&value, uint32_t &vlen);

	private:
		//���³�ʼ��
		int reInit();

		//�������ݿ�
		int connectTo(uint32_t n);

		//��ȡ����
		redisContext * applyContext();

		//�ͷ�����
		void releaseContext(redisContext * context);

		//��ʼ����
		void startConnect();

	private:
		TRedisHelper() :_ip("127.0.0.1"), _port(6379), _connect_count(1), _state(DB_STATUS_IDLE) {}
		TRedisHelper(TRedisHelper &r);

	private:
		static TRedisHelper * _instance;
		static CMutexLock * _mtx;
		static CCondLock * _cond;

	private:
		std::string _ip;                            /* ���ݿ�IP */
		uint16_t _port;                             /* ���ݿ�˿ں� */
		uint32_t _connect_count;                    /* ���ݿ������� */
		EN_DB_STATUS _state;                        /* ���ݿ�״̬ */
		std::vector<redisContext *> _contexts;      /* ���ݿ��������� */
		std::queue<redisContext *> _context_queue;  /* ���ݿ����Ӷ��� */
	};
}

#endif

