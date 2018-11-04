#include "main.h"

// Simple Win 32 api window class
//
class WinPlus
{
public:

	void ConstructWindow(HINSTANCE hInst, const TCHAR* szWindowTitle, const TCHAR* szWindowClass, int sizeX, int sizeY, DWORD dwStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE);
	int Start(int nShowCmd);

protected:

	HWND AddButtonControl(int iPosX, int iPosY, int iWidth, int iHeight, const int controlID, const TCHAR* label, DWORD dwStyle = BS_PUSHBUTTON | WS_CHILD | WS_VISIBLE | WS_TABSTOP);
	HWND AddEditControl(int iPosX, int iPosY, int iWidth, int iHeight, const int controlID, DWORD dwStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP);

	void AppendEditText(HWND hEditWnd, const TCHAR* text);

	LRESULT virtual CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	//void virtual OnPaint(HWND hWnd);
	void virtual OnCreate(HWND hWnd);
	//void virtual OnDestroy(HWND hWnd);

	HWND GetWindowHandle() const { return m_hWnd; }
	HINSTANCE GetInstance() const { return m_Inst; }

private:
	static LRESULT CALLBACK StaticWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	HINSTANCE m_Inst;
	HWND m_hWnd;

	bool m_bStarted = false;

	const TCHAR* szTitleName = TEXT("Default window");
};