
#ifdef TEST

#include "dbt_redis_operate.h"

#include <iostream>
#include <unistd.h>

using namespace std;

void test()
{
	int result = 0;
	databasetool::TRedisHelper::getInstance()->init();
	databasetool::TRedisHelper::getInstance()->set("ha", "hello world .");
	string strRes;
	result = databasetool::TRedisHelper::getInstance()->get("ha", strRes);
	if (result)
	{
		cout << "get error ." << endl;
		return;
	}

	cout << strRes << endl;
}

int main()
{
	while (true)
	{
		test();
		sleep(1);
	}
	
	getchar();
	return 0;
}

#endif // TEST

