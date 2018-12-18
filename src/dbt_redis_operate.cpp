
#include "dbt_redis_operate.h"
#include "dbt_errno.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <pthread.h>

namespace databasetool
{
	/* begin */

	/************************************************************************
	 TRedisHelper                                      
	************************************************************************/
	TRedisHelper* TRedisHelper::_instance = NULL;
	CMutexLock * TRedisHelper::_mtx = new CMutexLock;
	CCondLock * TRedisHelper::_cond = new CCondLock(_mtx);

	/********************************************************
	   Func Name: getInstance
	Date Created: 2018-10-12
	 Description: ����ʵ������
		   Input: 
		  Output: 
		  Return: ʵ������
		 Caution: 
	*********************************************************/
	TRedisHelper * TRedisHelper::getInstance()
	{
		TRedisHelper *tmp = NULL;

		if (NULL == _instance)
		{
			_mtx->lock();
			if (NULL == _instance)
			{
				tmp = new TRedisHelper();
				//���T��init����������main�����е���Init��������ȻInit����Ҳ�������⴫��
				_instance = tmp;
			}
			_mtx->unlock();
		}

		return _instance;
	}

	/********************************************************
	   Func Name: init
	Date Created: 2018-12-17
	 Description: ��ʼ��
		   Input: 
		  Output: 
		  Return: error code
		 Caution: 
	*********************************************************/
	int TRedisHelper::init(std::string ip, uint16_t port, uint32_t nConnect)
	{
		int result = 0;

		if (ip.empty() || 0 == nConnect)
		{
			return DEC_PARAM_FAIL;
		}
		//��ʼ��ֻ�ܵ���һ��
		if (DB_STATUS_IDLE != _state)
		{
			return DEC_FAIL;
		}
		_ip = ip;
		_port = port;
		_connect_count = nConnect;

		//���½�������
		result = connectTo(_connect_count);

		if (!result)
		{
			_state = DB_STATUS_RUN;
		}

		return 0;
	}

	/********************************************************
	   Func Name: reInit
	Date Created: 2018-12-17
	 Description: ���³�ʼ��
		   Input: 
		  Output: 
		  Return: error code
		 Caution: 
	*********************************************************/
	int TRedisHelper::reInit()
	{
		int result = 0;
		redisContext * tmpContext = NULL;
		std::vector<redisContext *>::iterator it;

		_mtx->lock();

		if (DB_STATUS_UNUSABLE != _state)
		{
			_mtx->unlock();
			return 0;
		}

		//1. �����������
		while (!_context_queue.empty())
		{
			_context_queue.pop();
		}
		for (it = _contexts.begin(); it != _contexts.end(); ++it)
		{
			tmpContext = *it;
			redisFree(tmpContext);
			tmpContext = NULL;
		}
		_contexts.clear();

		//���½�������
		result = connectTo(_connect_count);

		if (!result)
		{
			_state = DB_STATUS_RUN;
		}

		_mtx->unlock();

		return result;
	}

	/********************************************************
	   Func Name: connectTo
	Date Created: 2018-12-17
	 Description: �������ݿ�
		   Input: 
		  Output: 
		  Return: redisʵ��
		 Caution: 
	*********************************************************/
	int TRedisHelper::connectTo(uint32_t n)
	{
		int result = 0;
		redisContext * tmpContext = NULL;

		//2. �����µ�����
		for (uint32_t i = 0; i < _connect_count; i++)
		{
			tmpContext = redisConnect(_ip.c_str(), _port);
			if (NULL == tmpContext || tmpContext->err)
			{
				//printf("����Redisʧ��: %s\n", context->errstr);
				result = -1;
				break;
			}

			_contexts.push_back(tmpContext);
			_context_queue.push(tmpContext);
		}

		return result;
	}

	/********************************************************
	   Func Name: applyContext
	Date Created: 2018-12-17
	 Description: ��ȡ����
		   Input: 
		  Output: 
		  Return: redisʵ��
		 Caution: 
	*********************************************************/
	redisContext * TRedisHelper::applyContext()
	{
		int result = 0;
		redisContext * context = NULL;

		_mtx->lock();

		//����Ƿ����
		if (DB_STATUS_RUN != _state)
		{
			_mtx->unlock();
			return NULL;
		}

		while (_context_queue.empty())
		{
			result = _cond->timedwait(TIMEOUT_SPACE);
			if (result)
			{
				_mtx->unlock();
				return NULL;
				//break;
			}
		}
		context = _context_queue.front();
		_context_queue.pop();

		_mtx->unlock();

		return context;

	}

	/********************************************************
	   Func Name: releaseContext
	Date Created: 2018-12-17
	 Description: �ͷ�����
		   Input: 
		  Output: 
		  Return: 
		 Caution: 
	*********************************************************/
	void TRedisHelper::releaseContext(redisContext * context)
	{
		//int result = 0;

		_mtx->lock();

		_context_queue.push(context);
		_cond->signal();

		_mtx->unlock();

		return ;
	}

	/********************************************************
	   Func Name: reConnect
	Date Created: 2018-12-17
	 Description: ����
		   Input: 
		  Output: 
		  Return: 
		 Caution: 
	*********************************************************/
	void * TRedisHelper::reConnect(void *)
	{
		int result = 0;
		int nfd = 0;
		struct timeval stTimeval;
		
		do
		{
			//ÿ10s����һ��
			stTimeval.tv_sec = RECONNECT_LOOP;
			stTimeval.tv_usec = 0;
			//select()ÿ�η��غ󶼻����struct timeval����
			nfd = select(0, NULL, NULL, NULL, &stTimeval);
			//ִ������
			result = TRedisHelper::getInstance()->reInit();
			if (!result)
			{
				break;
			}
		} while ((0 == nfd) || (nfd < 0 && EINTR == errno));

		return NULL;
	}

	/********************************************************
	   Func Name: createThread
	Date Created: 2018-12-17
	 Description: ����
		   Input: 
		  Output: 
		  Return: 
		 Caution: 
	*********************************************************/
	void TRedisHelper::startConnect()
	{
		pthread_t thr;

		_mtx->lock();

		if (DB_STATUS_RUN != _state)
		{
			_mtx->unlock();
			return;
		}
		_state = DB_STATUS_UNUSABLE;

		_mtx->unlock();

		if (pthread_create(&thr,NULL,TRedisHelper::reConnect,NULL))
		{
			return;
		}
		pthread_detach(thr);
	}

	/********************************************************
	   Func Name: set
	Date Created: 2018-12-17
	 Description: set
		   Input: 
		  Output: 
		  Return: error code
		 Caution: 
	*********************************************************/
	int TRedisHelper::set(std::string key, std::string value)
	{
		int result = 0;
		redisReply *rep = NULL;
		redisContext *context = NULL;

		if (key.empty() || value.empty())
		{
			return DEC_PARAM_FAIL;
		}

		//��������
		context = applyContext();
		if (NULL == context)
		{
			return DEC_FAIL;
		}

		rep = (redisReply *)redisCommand(context, "set %s %s", key.c_str(), value.c_str());
		//�ͷ�����
		releaseContext(context);

		if (NULL == rep)
		{
			//˵������redisʧ�ܣ���Ҫ��������
			startConnect();
			return DEC_CONNECT_FAILED;
		}

		if (REDIS_REPLY_STATUS == rep->type)
		{
			if (0 != strncmp(rep->str, REDIS_ERROR_CODE_OK, strlen(REDIS_ERROR_CODE_OK)))
			{
				result = -1;
			}
		}

		freeReplyObject(rep);
		rep = NULL;

		return result;
	}

	/********************************************************
	   Func Name: setBinary
	Date Created: 2018-12-18
	 Description: ������set
		   Input: 
		  Output: 
		  Return: error code
		 Caution: 
	*********************************************************/
	int TRedisHelper::setBinary(std::string key, uint8_t *value, uint32_t vlen)
	{
		int result = 0;

		const char * argv[3] = { 0 };
		size_t argvlen[3] = { 0 };
		redisReply *rep = NULL;
		redisContext *context = NULL;

		if (key.empty() || NULL == value || 0 == vlen)
		{
			return DEC_PARAM_FAIL;
		}

		context = applyContext();
		if (NULL == context)
		{
			return DEC_FAIL;
		}

		argv[0] = "set";
		argvlen[0] = strlen("set");

		argv[1] = key.c_str();
		argvlen[1] = key.length();

		argv[2] = (char *)value;
		argvlen[2] = vlen;

		rep = (redisReply *)redisCommandArgv(context, 3, argv, argvlen);

		//�ͷ�����
		releaseContext(context);

		if (NULL == rep)
		{
			//˵������redisʧ�ܣ���Ҫ��������
			startConnect();
			return DEC_CONNECT_FAILED;
		}

		if (REDIS_REPLY_STATUS == rep->type)
		{
			if (0 != strncmp(rep->str, REDIS_ERROR_CODE_OK, strlen(REDIS_ERROR_CODE_OK)))
			{
				result = -1;
			}
		}

		freeReplyObject(rep);
		rep = NULL;

		return result;
	}

	/********************************************************
	   Func Name: setWithTimer
	Date Created: 2018-12-17
	 Description: ����ʱ��set
		   Input: 
		  Output: 
		  Return: error code
		 Caution: 
	*********************************************************/
	int TRedisHelper::setWithTimer(std::string key, std::string value, uint32_t second)
	{
		int result = 0;
		redisReply *rep = NULL;
		redisContext *context = NULL;

		if (key.empty() || value.empty())
		{
			return DEC_PARAM_FAIL;
		}

		context = applyContext();
		if (NULL == context)
		{
			return DEC_FAIL;
		}

		rep = (redisReply *)redisCommand(context, "set %s %s ex %u", key.c_str(), value.c_str(), second);

		//�ͷ�����
		releaseContext(context);

		if (NULL == rep)
		{
			//˵������redisʧ�ܣ���Ҫ��������
			startConnect();
			return DEC_CONNECT_FAILED;
		}

		if (REDIS_REPLY_STATUS == rep->type)
		{
			if (0 != strncmp(rep->str, REDIS_ERROR_CODE_OK, strlen(REDIS_ERROR_CODE_OK)))
			{
				result = -1;
			}
		}

		freeReplyObject(rep);
		rep = NULL;

		return result;
	}

	/********************************************************
	   Func Name: setBinaryWithTimer
	Date Created: 2018-12-18
	 Description: ����ʱ�Ķ�����set
		   Input: 
		  Output: 
		  Return: error code
		 Caution: 
	*********************************************************/
	int TRedisHelper::setBinaryWithTimer(std::string key, uint8_t *value, uint32_t vlen, uint32_t second)
	{
		int result = 0;

		const char * argv[5] = { 0 };
		size_t argvlen[5] = { 0 };
		redisReply *rep = NULL;
		redisContext *context = NULL;
		char gcTime[64] = { 0 };

		if (key.empty() || NULL == value || 0 == vlen)
		{
			return DEC_PARAM_FAIL;
		}

		context = applyContext();
		if (NULL == context)
		{
			return DEC_FAIL;
		}

		argv[0] = "set";
		argvlen[0] = strlen("set");

		argv[1] = key.c_str();
		argvlen[1] = key.length();

		argv[2] = (char *)value;
		argvlen[2] = vlen;

		//���ó�ʱʱ��
		argv[3] = "ex";
		argvlen[3] = strlen("ex");

		sprintf(gcTime, "%u", second);
		argv[4] = gcTime;
		argvlen[4] = strlen(gcTime);

		rep = (redisReply *)redisCommandArgv(context, 5, argv, argvlen);

		//�ͷ�����
		releaseContext(context);

		if (NULL == rep)
		{
			//˵������redisʧ�ܣ���Ҫ��������
			startConnect();
			return DEC_CONNECT_FAILED;
		}

		if (REDIS_REPLY_STATUS == rep->type)
		{
			if (0 != strncmp(rep->str, REDIS_ERROR_CODE_OK, strlen(REDIS_ERROR_CODE_OK)))
			{
				result = -1;
			}
		}

		freeReplyObject(rep);
		rep = NULL;

		return result;
	}

	/********************************************************
	   Func Name: get
	Date Created: 2018-12-18
	 Description: get
		   Input: 
		  Output: 
		  Return: error code
		 Caution: 
	*********************************************************/
	int TRedisHelper::get(std::string key, std::string &value)
	{
		int result = 0;
		redisReply *rep = NULL;
		redisContext *context = NULL;

		if (key.empty())
		{
			return DEC_PARAM_FAIL;
		}

		context = applyContext();
		if (NULL == context)
		{
			return DEC_FAIL;
		}

		rep = (redisReply *)redisCommand(context, "get %s", key.c_str());

		//�ͷ�����
		releaseContext(context);

		if (NULL == rep)
		{
			//˵������redisʧ�ܣ���Ҫ��������
			startConnect();
			return DEC_CONNECT_FAILED;
		}

		if (REDIS_REPLY_STRING == rep->type)
		{
			value = rep->str;
		}
		else
		{
			result = -1;
		}

		freeReplyObject(rep);
		rep = NULL;

		return result;
	}

	/********************************************************
	   Func Name: getBinary
	Date Created: 2018-12-18
	 Description: get
		   Input: 
		  Output: 
		  Return: error code
		 Caution: 
	*********************************************************/
	int TRedisHelper::getBinary(std::string key, uint8_t *&value, uint32_t &vlen)
	{
		//int result = 0;
		redisReply *rep = NULL;
		redisContext *context = NULL;

		if (key.empty())
		{
			return DEC_PARAM_FAIL;
		}

		context = applyContext();
		if (NULL == context)
		{
			return DEC_FAIL;
		}

		rep = (redisReply *)redisCommand(context, "get %s", key.c_str());

		//�ͷ�����
		releaseContext(context);

		if (NULL == rep)
		{
			//˵������redisʧ�ܣ���Ҫ��������
			startConnect();
			return DEC_CONNECT_FAILED;
		}

		vlen = rep->len;
		if (rep->len <= 0)
		{
			return -1;
		}
		value = (uint8_t *)malloc(vlen);
		if (NULL == value)
		{
			return -1;
		}
		memset(value, 0, vlen);
		memcpy(value, rep->str, rep->len);

		freeReplyObject(rep);
		rep = NULL;

		return 0;
	}


	/* end */
}




