#include "Winpp.h"
#include "resource.h"

#pragma comment(lib, "Ws2_32.lib")

static TCHAR szSvPort[16];
static TCHAR szConnectPort[16];

static TCHAR szConnectIP[64];
static ATOM m_ServerStopFlag = 0;
static ATOM m_ClientRunning = 0;

class MyWindow : public WinPlus
{
protected:
	
	LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) override
	{
		switch (message)
		{
			case WM_PAINT:
			{
				OnPaint(hWnd);
			}
			break;
			case WM_CLOSE:
				OnDestroy(hWnd);
				PostQuitMessage(0);
				break;

			case WM_COMMAND:
				switch (LOWORD(wParam))
				{
				case ID_BTN_CREATESERVER:
					if (!IsServerRunning())
					{
						CreateServer(hWnd);
					}
					else
						MessageBox(hWnd, TEXT("Started allready"), TEXT("Server"), MB_OK | MB_ICONINFORMATION);
					break;
				case ID_BTN_DISCONNECT:
					Disconnect();
					break;
				case ID_BTN_CLIENTCONNECT:
					ConnectToServer(hWnd);
					break;
				case ID_BTN_SEND:
					Send();
					break;
				}
			default:
				return DefWindowProc(hWnd, message, wParam, lParam);
		}
		return 0;
	}

	void OnCreate(HWND hWnd) override
	{
		// Init Winsock
		int iRes = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (iRes != 0)
		{
			MessageBox(NULL, TEXT("WSAStartup failed"), TEXT("WSAStartup Error"), MB_OK | MB_ICONERROR);
			return;
		}

		// Add controls
		AddButtonControl(7, 410, 120, 24, ID_BTN_CREATESERVER, TEXT("Create server"));
		AddButtonControl(133, 410, 120, 24, ID_BTN_CLIENTCONNECT, TEXT("Connect to server"));
		AddButtonControl(259, 410, 120, 24, ID_BTN_DISCONNECT, TEXT("Disconnect"));
		AddButtonControl(540, 380, 80, 24, ID_BTN_SEND, TEXT("Send"));

		hEditLogs = AddEditControl(5, 5, 615, 370, ID_EDIT_LOGS,
			ES_READONLY | ES_MULTILINE | ES_AUTOVSCROLL | WS_CHILD | WS_VISIBLE | WS_VSCROLL);

		hEditSend = AddEditControl(5, 380, 530, 24, ID_EDIT_MESSAGE, WS_CHILD | WS_VISIBLE);

		_tcscpy_s(szSvPort, TEXT("26555"));
		_tcscpy_s(szConnectPort, TEXT("26555"));
		_tcscpy_s(szConnectIP, TEXT("127.0.0.1"));	// localhost
	}

private:

	bool IsServerRunning()
	{
		if (m_ServerSocket != INVALID_SOCKET)
			return true;

		return false;
	}

	bool IsClientConnected()
	{
		if (m_ClientRunning == 1)
			return true;

		return false;
	}

	void CreateServer(HWND hWnd)
	{
		if (IsClientConnected())	// Disconnect first
			return;

		if (IsServerRunning())
		{
			if (MessageBox(hWnd, TEXT("Server is running. Stop?"), TEXT("Server"), MB_YESNO | MB_ICONWARNING) == IDYES)
			{
				Disconnect();
			}
			else
			{
				return;
			}
		}

		int iRes;
		iRes = DialogBox(GetInstance(), MAKEINTRESOURCE(IDD_CREATESERVER), hWnd, WndSvDialog);

		if (iRes == 0)		// Canceled
			return;

		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		hints.ai_flags = AI_PASSIVE;

		// Resolve the local address and port to be used by the server
		iRes = getaddrinfo(NULL, szSvPort, &hints, &result);
		if (iRes != 0)
		{
			MessageBox(NULL, TEXT("getaddrinfo failed"), TEXT("Server Error"), MB_OK | MB_ICONERROR);
			return;
		}

		// Create a SOCKET for connecting to server
		m_ServerSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
		if (m_ServerSocket == INVALID_SOCKET)
		{
			MessageBox(NULL, TEXT("Failed to initialize server socket"), TEXT("Server Error"), MB_OK | MB_ICONERROR);
			freeaddrinfo(result);
			return;
		}

		// Setup the TCP listening socket
		iRes = bind(m_ServerSocket, result->ai_addr, (int)result->ai_addrlen);
		if (iRes == SOCKET_ERROR)
		{
			MessageBox(NULL, TEXT("Bind failed"), TEXT("Server Error"), MB_OK | MB_ICONERROR);
			freeaddrinfo(result);
			closesocket(m_ServerSocket);
			return;
		}

		freeaddrinfo(result);

		iRes = listen(m_ServerSocket, SOMAXCONN);
		if (iRes == SOCKET_ERROR)
		{
			MessageBox(NULL, TEXT("Failed to set listen socket"), TEXT("Server Error"), MB_OK | MB_ICONERROR);
			closesocket(m_ServerSocket);
			return;
		}

		if (m_ServerAccept.joinable())
			m_ServerAccept.join();

		if (m_ReadThread.joinable())
			m_ReadThread.join();

		m_ServerStopFlag = 0;

		m_ServerAccept = std::thread(&MyWindow::ServerAcceptThread, this);
		m_ReadThread = std::thread(&MyWindow::ServerReadThread, this);

		LogSafe(TEXT("Starting server\r\n"));
	}

	void ConnectToServer(HWND hWnd)
	{
		if (IsClientConnected())	// Disconnect first
			return;

		int iRes;

		iRes = DialogBox(GetInstance(), MAKEINTRESOURCE(IDD_CONNECT), hWnd, WndClDialog);

		if (iRes == 0)		// Canceled
			return;

		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		// Resolve the server address and port
		iRes = getaddrinfo(szConnectIP, szConnectPort, &hints, &result);
		if (iRes != 0)
		{
			LogSafe(TEXT("Failed to resolve the server address\r\n"));
			return;
		}

		if (m_ClientThread.joinable())
			m_ClientThread.join();

		m_ClientRunning = 1;
		m_ClientThread = std::thread(&MyWindow::ClientThread, this);
	}

	void Disconnect()
	{
		if (IsServerRunning())
		{
			m_ServerStopFlag = 1;

			if (m_ServerSocket != INVALID_SOCKET)
			{
				closesocket(m_ServerSocket);
				m_ServerSocket = INVALID_SOCKET;
			}

			if (m_AcceptSocket != INVALID_SOCKET)
			{
				closesocket(m_AcceptSocket);
				m_AcceptSocket = INVALID_SOCKET;
			}

			if (m_ServerAccept.joinable())
				m_ServerAccept.join();

			if (m_ReadThread.joinable())
				m_ReadThread.join();

			m_Clients.clear();

			LogSafe(TEXT("Server shutting down\r\n"));
		}

		if (IsClientConnected())
		{
			ClientDisconnect();
		}
	}

	void ClientDisconnect()
	{
		m_ClientRunning = 0;

		if (m_ClientSocket != INVALID_SOCKET)
		{
			closesocket(m_ClientSocket);
			m_ClientSocket = INVALID_SOCKET;
		}

		if (m_ClientThread.joinable())
			m_ClientThread.join();
	}

	void Send()
	{
		TCHAR buf[DEFAULT_BUFLEN]{ 0 };
		SendMessage(hEditSend, WM_GETTEXT, DEFAULT_BUFLEN - 1, reinterpret_cast<LPARAM>(buf));
		int len = _tcslen(buf) * sizeof(TCHAR);

		{
			std::unique_lock<std::mutex> lock{ m_SendMutex };

			if (IsClientConnected())
			{
				send(m_ClientSocket, buf, len, 0);
			}

			if (IsServerRunning())
			{
				for (auto s : m_Clients)
					send(s, buf, len, 0);
			}
		}

		SendMessage(hEditSend, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(TEXT("")));
	}

	// Thread safe plz
	void RemoveClient(std::size_t at)
	{
		if (m_Clients[at] != INVALID_SOCKET)
			closesocket(m_Clients[at]);

		m_Clients.erase(m_Clients.begin() + at);
	}

	void OnPaint(HWND hWnd)
	{
		HDC hDC;
		PAINTSTRUCT ps;

		hDC = BeginPaint(hWnd, &ps);

		//Rectangle(hDC, 5, 5, 100, 50);

		EndPaint(hWnd, &ps);
	}

	void OnDestroy(HWND hWnd)
	{
		ClientDisconnect();

		Disconnect();

		WSACleanup();
	}

	INT_PTR static CALLBACK WndSvDialog(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (uMsg)
		{
		case WM_INITDIALOG:
		{
			SendMessage(GetDlgItem(hWnd, IDC_EDITSVPORT), WM_SETTEXT, 0, reinterpret_cast<LPARAM>(szSvPort));
		}
		break;
		case WM_CLOSE:
			EndDialog(hWnd, 0);
			return TRUE;
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
			case IDCANCEL:
				EndDialog(hWnd, 0);
				return TRUE;
			case IDOK:
				SendMessage(GetDlgItem(hWnd, IDC_EDITSVPORT), WM_GETTEXT, 16, reinterpret_cast<LPARAM>(szSvPort));
				EndDialog(hWnd, 1);
				return TRUE;
			}
			break;
		}

		return FALSE;
	}

	INT_PTR static CALLBACK WndClDialog(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (uMsg)
		{
		case WM_INITDIALOG:
		{
			SendMessage(GetDlgItem(hWnd, IDC_EDT_CON_PORT), WM_SETTEXT, 0, reinterpret_cast<LPARAM>(szConnectPort));
			SendMessage(GetDlgItem(hWnd, IDC_EDT_CON_IP), WM_SETTEXT, 0, reinterpret_cast<LPARAM>(szConnectIP));
		}
		break;
		case WM_CLOSE:
			EndDialog(hWnd, 0);
			return TRUE;
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
			case IDCANCEL:
				EndDialog(hWnd, 0);
				return TRUE;
			case IDOK:
				SendMessage(GetDlgItem(hWnd, IDC_EDT_CON_PORT), WM_GETTEXT, 16, reinterpret_cast<LPARAM>(szConnectPort));
				SendMessage(GetDlgItem(hWnd, IDC_EDT_CON_IP), WM_GETTEXT, 64, reinterpret_cast<LPARAM>(szConnectIP));
				EndDialog(hWnd, 1);
				return TRUE;
			}
			break;
		}

		return FALSE;
	}

	void ServerAcceptThread()
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));

		while (m_ServerStopFlag == 0)
		{
			// Accept a client socket
			m_AcceptSocket = accept(m_ServerSocket, NULL, NULL);
			if (m_AcceptSocket == INVALID_SOCKET)
			{
				continue;
			}

			LogSafe(TEXT("Client connected\r\n"));

			{
				std::unique_lock<std::mutex> lock{ m_Mutex };

				m_Clients.push_back(m_AcceptSocket);
			}
		}

		return;
	}

	void ServerReadThread()
	{
		TCHAR buf[DEFAULT_BUFLEN]{ 0 };
		int iRes, iSend;
		u_long iBL;

		std::this_thread::sleep_for(std::chrono::seconds(1));

		while (m_ServerStopFlag == 0)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));

			//for (auto s : m_Clients)
			for (int i = 0; i < m_Clients.size(); i++)
			{
				{
					std::unique_lock<std::mutex> lock{ m_Mutex };

					iRes = ioctlsocket(m_Clients[i], FIONREAD, &iBL);

					if (iRes != NO_ERROR)
					{
						RemoveClient(i);
						continue;
					}

					if (iBL == 0)
					{
						std::this_thread::sleep_for(std::chrono::milliseconds(25));
						continue;
					}

					iRes = recv(m_Clients[i], reinterpret_cast<char*>(buf), DEFAULT_BUFLEN * sizeof(TCHAR) - 6, 0);
					
					if (iRes > 0)
					{
						Log(buf);
						Log(TEXT("\r\n"));
						// Send buffer to every connected client
						//for (auto s : m_Clients)
						for (int isend = 0; isend < m_Clients.size(); isend++)
						{
							iSend = send(m_Clients[isend], buf, iRes, 0);
						}
					}
					else if (iRes == 0)
					{
						RemoveClient(i);
					}
				}
			}
		}

		return;
	}

	void ClientThread()
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));

		TCHAR buf[DEFAULT_BUFLEN]{ 0 };		// String

		struct addrinfo *ptr = NULL;

		bool bIsConnected = true;
		int iRes, connAttempt = 0;		// Connect attempt counter
		u_long iBL;

		for (ptr = result; connAttempt < 5; connAttempt++)
		{
			// Create a SOCKET for connecting to server
			m_ClientSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

			if (m_ClientSocket == INVALID_SOCKET)
			{
				LogSafe(TEXT("Failed to initialize client socket\r\n"));
				freeaddrinfo(result);
				m_ClientRunning = 0;
				return;
			}

			// Connect to server.
			iRes = connect(m_ClientSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
			if (iRes == SOCKET_ERROR)
			{
				closesocket(m_ClientSocket);
				m_ClientSocket = INVALID_SOCKET;

				std::this_thread::sleep_for(std::chrono::seconds(2));		// wait 2s till next attempt to connect
				LogSafe(TEXT("Retry connection..\r\n"));
				continue;
			}

			break;
		}

		freeaddrinfo(result);

		if (m_ClientSocket == INVALID_SOCKET)
		{
			LogSafe(TEXT("Connection failed after 5 retries\r\n"));
			m_ClientRunning = 0;
			return;
		}

		LogSafe(TEXT("Connected\r\n"));

		while (m_ClientRunning == 1)
		{
			iRes = ioctlsocket(m_ClientSocket, FIONBIO, &iBL);
			if (iRes != NO_ERROR)
			{
				m_ClientRunning = 0;
				if (m_ClientSocket != INVALID_SOCKET)
				{
					closesocket(m_ClientSocket);
					m_ClientSocket = INVALID_SOCKET;
				}
				return;
			}

			if (iBL == 0)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(25));
				continue;
			}

			iRes = recv(m_ClientSocket, reinterpret_cast<char*>(buf), DEFAULT_BUFLEN * sizeof(TCHAR) - 6, 0);

			if (iRes > 0)	// Convert buffer to text
			{
				_tcscat_s(buf, TEXT("\r\n"));
				LogSafe(buf);
			}
		}

		return;
	}

	void Log(const TCHAR* szLogText)
	{
		AppendEditText(hEditLogs, szLogText);
	}

	void LogSafe(const TCHAR* szLogText)
	{
		std::unique_lock<std::mutex> lock{ m_Mutex };

		AppendEditText(hEditLogs, szLogText);
	}

	static const unsigned int DEFAULT_BUFLEN = 1024;

	static const unsigned int ID_BTN_CREATESERVER	= 500;
	static const unsigned int ID_BTN_CLIENTCONNECT	= 501;
	static const unsigned int ID_BTN_DISCONNECT		= 502;
	static const unsigned int ID_BTN_SEND			= 503;

	static const unsigned int ID_EDIT_MESSAGE		= 504;
	static const unsigned int ID_EDIT_LOGS			= 505;

	WSADATA wsaData;

	SOCKET m_ServerSocket = INVALID_SOCKET, m_AcceptSocket = INVALID_SOCKET,
		m_ClientSocket = INVALID_SOCKET, m_ReadSocket = INVALID_SOCKET;

	struct addrinfo *result = NULL, hints;

	// Controll handles
	HWND hEditSend, hEditLogs;
	HWND hBtnSend;

	std::vector<SOCKET> m_Clients;
	std::thread m_ServerAccept;
	std::thread m_ReadThread;
	std::thread m_ClientThread;

	std::mutex m_Mutex, m_LogMutex, m_SendMutex;
};

















int __stdcall _tWinMain(HINSTANCE hInst, HINSTANCE hPrevInst, TCHAR* pCmd, int iShowCmd)
{
	int iRet;
	MyWindow wnd;

	wnd.ConstructWindow(hInst, TEXT("Sockets"), TEXT("CDemo"), 640, 480, WS_CAPTION | WS_VISIBLE | WS_SYSMENU);
	iRet = wnd.Start(iShowCmd);

	return iRet;
}
