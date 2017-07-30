// messageApp.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <Windows.h>
#include <vector>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <limits>
#include <map>
#include <queue>
#include <thread>
#include <functional>
#include "gtest/gtest.h"

using namespace std;
using ::testing::EmptyTestEventListener;
using ::testing::InitGoogleTest;
using ::testing::Test;
using ::testing::TestCase;
using ::testing::TestEventListeners;
using ::testing::TestInfo;
using ::testing::TestPartResult;
using ::testing::UnitTest;



class CMsgCallback;

const char red[] = "red";
const char blue[] = "blue";
const char yellow[] = "yellow";
static const int num_threads = 5;
static bool g_bShutDown = false;

class CMsgObject
{
public:
	CMsgObject() {
		m_priority = 0;
		memset(m_msgType, 0, sizeof(m_msgType));
		memset(m_message, 0, sizeof(m_message));
		m_callback_id = 0;
	}
	CMsgObject(std::string type, int priority, std::string message, int callback_id = 0)
	{
		m_priority = priority;
		strncpy_s(m_msgType, sizeof(m_msgType), type.c_str(), _TRUNCATE);
		strncpy_s(m_message, sizeof(m_message), message.c_str(), _TRUNCATE);
		m_callback_id = callback_id;

	}

	~CMsgObject() {
	}
	int  m_priority;
	char m_msgType[7];
	char m_message[80];
	int m_callback_id;
};

// using a map of priority order queue so that all priority n items can be saved in the same queue
typedef std::map<int, std::queue<CMsgObject> > TData;
class CMsgReceiver
{
public:
	CMsgReceiver() {}
	~CMsgReceiver() {}

	bool get(std::string msgType, std::string &message, int& iCallBackID)
	{
		if (msgType.length() == 0)
			return false;
		
		if (_stricmp(msgType.c_str(), red) == 0)
			return GetQueueItem(mRedQueue, message, iCallBackID);
		else if (_stricmp(msgType.c_str(), blue) == 0)
			return GetQueueItem(mBlueQueue, message, iCallBackID);
		else if (_stricmp(msgType.c_str(), yellow) == 0)
			return GetQueueItem(mYellowQueue, message, iCallBackID);

		return false;
	};

	bool add(CMsgObject &msg)
	{
		if (msg.m_priority <= 0 || msg.m_priority > 3)
		{
			cout << "ERROR-INVALID-MSG-PRIORITY: " << msg.m_priority << std::endl;
			return false;
		}
		if (_stricmp(msg.m_msgType, red) == 0)
			mRedQueue[msg.m_priority].push(msg);
		else if (_stricmp(msg.m_msgType, blue) == 0)
			mBlueQueue[msg.m_priority].push(msg);
		else if (_stricmp(msg.m_msgType, yellow) == 0)
			mYellowQueue[msg.m_priority].push(msg);
		else
		{
			cout << "ERROR-UNKNOWN-MSG-TYPE " << msg.m_msgType << endl;
			return false;
		}
		return true;
	}

protected:
	bool GetQueueItem(TData &mq, std::string &message, int& iCallBackID)
	{
		if (mq.empty())
			return false;

		CMsgObject data;
		if (!mq[3].empty())
		{
			data = mq[3].front();
			message = data.m_message;
			iCallBackID = data.m_callback_id;
			mq[3].pop();
			return true;
		}

		if (!mq[2].empty())
		{
			data = mq[2].front();
			message = data.m_message;
			iCallBackID = data.m_callback_id;
			mq[2].pop();
			return true;
		}

		if (!mq[1].empty())
		{
			data = mq[1].front();
			message = data.m_message;
			iCallBackID = data.m_callback_id;
			mq[1].pop();
			return true;
		}

		return false;
	};

public:
	TData mRedQueue;
	TData mBlueQueue;
	TData mYellowQueue;

	

};

class CMsgSender
{

};


static CMsgReceiver _receiver;
CRITICAL_SECTION g_critSec;

// public interface for use in sending messages within this program.
// using callback_id as int as opposed to address of callback object as it is safer -- obect sending message shouldn't
// need not worry about the lifetime of the callback object since this call could easily be made accross a network 
// over a network for communication I would use a char buffer with a fixed header structure, so I can send multiple messages in 1 call
void send_msg(std::string msgType, int priority, std::string data, int callback_id)
{
	EnterCriticalSection(&g_critSec);
	if (!_receiver.add(CMsgObject(msgType, priority, data, callback_id)))
	{
		cout << "ERROR-MESSAGE-NOT-SENT " << endl;
	}
	LeaveCriticalSection(&g_critSec);
}

void sendmsg_from_thread(int tid) {
	int iSleep = (rand() % 20) * 500;
	string msgT = "unknown";
	int priority = 5;
	switch (tid)
	{
	case 1: priority = 3; msgT = "red"; break;
	case 2: priority = 2; msgT = "blue"; break;
	case 3: priority = 1; msgT = "yellow"; break;
	case 0: priority = 2; msgT = "red"; break;
	default: priority = 999; msgT = "invalid"; break;

	}

	std::cout << "Launched by thread " << tid << " with wait interval " << iSleep << " - " << priority << ":" << msgT << std::endl;
	int ctr = 0;
	while (!g_bShutDown)
	{
		++ctr;
		std::stringstream ss;
		ss << "msg #" << ctr << " from tid " << tid << " " << msgT << ":" << priority;
		Sleep(iSleep);

		string sData = ss.str();
		cout << sData << endl << endl;
		send_msg(msgT, priority, sData, tid);
	}

	std::cout << "exiting thread " << tid << std::endl;

}

int main()
{
	std::thread t[num_threads];

	// Initialize the critical section one time only.
	if (!InitializeCriticalSectionAndSpinCount(&g_critSec, 0x00000400))
		return EXIT_FAILURE;

	std::wstring sFileName = L"messageApp.exe";
	std::vector<const TCHAR*> vecArgs;

	vecArgs.push_back(sFileName.c_str());   // save the filename 
	int nargs = vecArgs.size();

	::testing::InitGoogleTest(&nargs, const_cast<TCHAR**>(vecArgs.data()));
	int rc = RUN_ALL_TESTS();

	// main part of program .. 


	//Launch threads to send messages
	for (int i = 0; i < num_threads; ++i) {
		t[i] = std::thread(sendmsg_from_thread, i);
	}

	// allow threads to queue messages for processing
	Sleep(15000);

	//wait for threads to exit ..
	g_bShutDown = true;
	for (int i = 0; i < num_threads; ++i) {
		t[i].join();
	}

	// get messages from queues for dispatch
	string sRed, sBlue, sYellow;
	int r_id = 0, b_id = 0, y_id = 0;

	// at this point -- when we retrieve message we can check if the callback address is valid for sendig message
	// if not valid -- we raise some kind of exception .. I am assumming that call back object exists .. 
	if (_receiver.get(red, sRed, r_id))
		cout << "retrieved msg: " << sRed << " for callback << " << r_id << endl;
	if (_receiver.get(blue, sBlue, b_id))
		cout << "retrieved msg: " << sBlue << " for callback << " << b_id << endl;
	if (_receiver.get(yellow, sYellow, y_id))
		cout << "retrieved msg: " << sYellow << " for callback << " << y_id << endl;


	// Release ownership of the critical section.
	LeaveCriticalSection(&g_critSec);

	return EXIT_SUCCESS;
}


// start google test section 

TEST(AddItemTest, add_to_red_queue)
{
	CMsgObject a("red", 1, "red");
	CMsgReceiver r;

	r.add(a);
	EXPECT_EQ(1, r.mRedQueue[1].size());
	EXPECT_TRUE(r.mBlueQueue.empty());
	EXPECT_TRUE(r.mYellowQueue.empty());
}

TEST(AddItemTest, add_to_blue_queue)
{
	CMsgObject a("blue", 1, "blue");
	CMsgReceiver r;

	r.add(a);
	EXPECT_EQ(1, r.mBlueQueue[1].size());
	EXPECT_TRUE(r.mRedQueue.empty());
	EXPECT_TRUE(r.mYellowQueue.empty());


}

TEST(AddItemTest, add_to_yellow_queue)
{
	CMsgObject a("yellow", 1, "yellow");
	CMsgReceiver r;

	r.add(a);
	EXPECT_EQ(1, r.mYellowQueue[1].size());
	EXPECT_TRUE(r.mRedQueue.empty());
	EXPECT_TRUE(r.mBlueQueue.empty());
}

TEST(AddItemTest, add_to_queue)
{
	CMsgObject a("yellow", 1, "yellow");
	CMsgReceiver r;

	r.add(CMsgObject("YELLOW", 1, "yellow"));
	r.add(CMsgObject("RED", 1, "red"));
	r.add(CMsgObject("BLUE", 1, "blue"));
	EXPECT_EQ(1, r.mBlueQueue[1].size());
	EXPECT_EQ(1, r.mRedQueue[1].size());
	EXPECT_EQ(1, r.mYellowQueue[1].size());
}

TEST(GetItemTest, add_to_queue_then_verify_queue_empty)
{
	CMsgObject a("yellow", 1, "yellow");
	CMsgReceiver r;

	r.add(CMsgObject("YELLOW", 1, "yellow"));
	r.add(CMsgObject("RED", 1, "red"));
	r.add(CMsgObject("BLUE", 1, "blue"));
	EXPECT_EQ(1, r.mBlueQueue[1].size());
	EXPECT_EQ(1, r.mRedQueue[1].size());
	EXPECT_EQ(1, r.mYellowQueue[1].size());

	std::string msg;
	int iCallback;
	EXPECT_TRUE(r.get("blue", msg, iCallback));
	EXPECT_TRUE(r.get("YeLLow", msg, iCallback));
	EXPECT_TRUE(r.get("reD", msg, iCallback));
	EXPECT_EQ(0, r.mBlueQueue[1].size());
	EXPECT_EQ(0, r.mRedQueue[1].size());
	EXPECT_EQ(0, r.mYellowQueue[1].size());

}

TEST(GetItemTest, add_to_queue_then_verify_priority_order)
{
	CMsgObject a("yellow", 1, "yellow");
	CMsgReceiver r;

	r.add(CMsgObject("red", 1, "1"));
	r.add(CMsgObject("RED", 2, "2"));
	r.add(CMsgObject("rEd", 3, "3"));
	r.add(CMsgObject("rEd", 2, "4"));
	r.add(CMsgObject("rEd", 3, "5"));

	// expecting msgs in following order: 3, 5, 2, 4 , 1
	EXPECT_EQ(2, r.mRedQueue[3].size());
	EXPECT_EQ(2, r.mRedQueue[2].size());
	EXPECT_EQ(1, r.mRedQueue[1].size());

	std::string msg;
	int iCallback;

	EXPECT_TRUE(r.get("reD", msg, iCallback));
	EXPECT_STREQ("3", msg.c_str());

	EXPECT_TRUE(r.get("reD", msg, iCallback));
	EXPECT_STREQ("5", msg.c_str());

	EXPECT_TRUE(r.get("reD", msg, iCallback));
	EXPECT_STREQ("2", msg.c_str());

	EXPECT_TRUE(r.get("reD", msg, iCallback));
	EXPECT_STREQ("4", msg.c_str());

	EXPECT_TRUE(r.get("reD", msg, iCallback));
	EXPECT_STREQ("1", msg.c_str());


	EXPECT_EQ(0, r.mRedQueue[3].size());
	EXPECT_EQ(0, r.mRedQueue[2].size());
	EXPECT_EQ(0, r.mRedQueue[1].size());

}
// stop google test section
