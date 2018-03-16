#include "stdafx.h"
#include "CppUnitTest.h"
#include "simplewebsocket.h"
#include <thread>
#include <chrono>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace mixer;

int numMessages = 1000;
int numMessagesSent = 0;
int numMessagesReceived = 0;

namespace simplewebsocket_test
{
TEST_CLASS(UnitTest1)
{
public:

	TEST_METHOD(TestMethod1)
	{
		
		websocket_handle handle;
		int error = 0;
		Logger::WriteMessage("Creating websocket.");
		error = create_websocket(&handle);
		Assert::IsTrue(0 == error);
		auto onConnect = [](const websocket_handle socket, const char* connectMessage, const unsigned int connectMessageSize)
		{
			Logger::WriteMessage(connectMessage);
		};

		auto onMessage = [](const websocket_handle socket, const char* message, const unsigned int messageSize)
		{	
			Logger::WriteMessage(message);
			++numMessagesReceived;
			int sleepTimeMs = rand() % 100;
			std::this_thread::sleep_for(std::chrono::milliseconds(sleepTimeMs));
		};

		auto onError = [](const websocket_handle socket, const unsigned short errorCode, const char* errorMessage, const unsigned int errorMessageSize)
		{
			Logger::WriteMessage(("ERROR (" + std::to_string(errorCode) + ")" + errorMessage).c_str());
		};

		auto onClose = [](const websocket_handle socket, const unsigned short code, const char* reason, const unsigned int reasonSize)
		{
			Logger::WriteMessage(("CLOSED (" + std::to_string(code) + ")" + reason).c_str());
		};

		Logger::WriteMessage("Opening websocket.");
		std::thread t([&]
		{
			int error = open_websocket(handle, "wss://echo.websocket.org", onConnect, onMessage, onError, onClose);
		});

		for (int i = 0; i < numMessages; ++i)
		{	
			int sleepTimeMs = rand() % 100;
			std::string message = "Test message: " + std::to_string(i);
			Logger::WriteMessage(("Sending message to server: " + message).c_str());
			error = write_websocket(handle, message.c_str());
			std::this_thread::sleep_for(std::chrono::milliseconds(sleepTimeMs));
			Assert::IsTrue(0 == error);
			++numMessagesSent;
		}

		// Give the socket time to finish up before closing it.
		std::this_thread::sleep_for(std::chrono::milliseconds(1000 * 5));

		Logger::WriteMessage("Closing websocket.");
		error = close_websocket(handle);
		Assert::IsTrue(0 == error);
		t.join();

		Assert::IsTrue(numMessagesReceived == numMessagesSent);
	}
};
}