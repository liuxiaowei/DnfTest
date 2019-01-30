#pragma once
#include "opencv/cv.h"
#include "opencv/cxcore.h"
#include "opencv/highgui.h"
class account_info;
using namespace std;  
using namespace cv;

class CGameControl
{
public:
	CGameControl(HWND hShow);
	~CGameControl(void);
	void ClickLoginInArea();
	void StartGame();
	void InputCodes();
	void CreateRole();
	void EndGame();
	void SetAccountIndex(const int& index);
	bool FindCurrentAccountIndex();
	void Stop();
	void GameProcess();
private:
	BOOL IsCanCreateRoles();
	void ClickAgreement();
	BOOL ImageMatchFromHwnd(HWND hWnd,const TCHAR* ImagePath,float fSame,
		OUT int& nX,OUT int& nY,bool bSave, bool bGray = true /*是否用灰度图比较*/);
	IplImage* HBitmapToLpl(HBITMAP hBmp);
	BOOL FindImageInGameWnd(const string& image, float fSame = 0.7, bool bGray = true);
	BOOL FindImageInLoginWnd(const string& image);
	string CreateName(const unsigned int & count);
	void SelectProfession();
	BOOL KillProcess(const string& processName);
private:
	HWND m_hShow;
	int	m_Index;
	bool m_Stop;
};

