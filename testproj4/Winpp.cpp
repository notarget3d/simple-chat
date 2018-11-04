#include "Winpp.h"

void WinPlus::ConstructWindow(HINSTANCE hInst, const TCHAR* szWindowTitle, const TCHAR* szWindowClass, int sizeX, int sizeY, DWORD dwStyle)
{
	WNDCLASSEX wndclx;
	ZeroMemory(reinterpret_cast<void*>(&wndclx), sizeof(WNDCLASSEX));

	wndclx.cbSize = sizeof(WNDCLASSEX);
	wndclx.hbrBackground = GetSysColorBrush(GRAY_BRUSH);
	wndclx.hCursor = NULL;
	wndclx.hIcon = NULL;
	wndclx.hIconSm = NULL;
	wndclx.lpfnWndProc = 0;
	wndclx.hInstance = hInst;
	wndclx.lpszClassName = szWindowClass;
	wndclx.lpfnWndProc = StaticWndProc;

	if (!RegisterClassEx(&wndclx))
	{
		throw std::exception("Error! Failed to register window class", GetLastError());
	}

	m_hWnd = CreateWindowEx(WS_EX_COMPOSITED, wndclx.lpszClassName,
		szWindowTitle, dwStyle,
		0, 0, sizeX, sizeY, NULL, 0, hInst, reinterpret_cast<WinPlus*>(this));

	if (m_hWnd == NULL)
	{
		throw std::exception("Error! Failed to register window class", GetLastError());
	}

	OnCreate(m_hWnd);
	m_Inst = hInst;

	// Set window long ptr to this class instance so window procedure can have access to it
	SetWindowLongPtr(m_hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
}

int WinPlus::Start(int nShowCmd)
{
	if (m_bStarted)
		return 0;

	ShowWindow(m_hWnd, nShowCmd);
	m_bStarted = true;

	// Run the message loop.
	MSG msg = {};
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK WinPlus::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CLOSE:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

void WinPlus::OnCreate(HWND hWnd) { return; }

void WinPlus::AppendEditText(HWND hEditWnd, const TCHAR* text)
{
	DWORD StartPos, EndPos;
	int outLength;
	SendMessage(hEditWnd, EM_GETSEL, reinterpret_cast<WPARAM>(&StartPos),			// Get current selection
		reinterpret_cast<WPARAM>(&EndPos));											// to restore later

	outLength = GetWindowTextLength(hEditWnd);										// Lenght for last char
	SendMessage(hEditWnd, EM_SETSEL, outLength, outLength);							// select last char
	SendMessage(hEditWnd, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(text));		// replace where selected
	SendMessage(hEditWnd, EM_SETSEL, StartPos, EndPos);								// restore selection
}

HWND WinPlus::AddButtonControl(int iPosX, int iPosY, int iWidth, int iHeight, const int controlID, const TCHAR* label, DWORD dwStyle)
{
	return CreateWindow(WC_BUTTON, label, dwStyle, iPosX, iPosY, iWidth, iHeight, m_hWnd,
		(HMENU)controlID, m_Inst, NULL);
}

HWND WinPlus::AddEditControl(int iPosX, int iPosY, int iWidth, int iHeight, const int controlID, DWORD dwStyle)
{
	return CreateWindow(WC_EDIT, TEXT(""), dwStyle, iPosX, iPosY, iWidth, iHeight, m_hWnd,
		(HMENU)controlID, m_Inst, NULL);
}

LRESULT CALLBACK WinPlus::StaticWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	WinPlus* wndp = reinterpret_cast<WinPlus*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

	if (wndp)
		return wndp->WndProc(hWnd, msg, wParam, lParam);

	return DefWindowProc(hWnd, msg, wParam, lParam);
}
