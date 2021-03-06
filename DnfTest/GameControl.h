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
	void SelectAreaProcess();
	bool StartGame();
	bool InputCodes();
	bool CreateRole();
	void Stop();
	void SelectProfession();
	void SelectArea();
	void GameLoop();
	void EndGame();
	void FindRolePos();
	void Fight();
	void FightLinDong();
private:
	void outputFile();
	void resetIndex();
	void setAccountIndex(const int& index);
	bool findCurrentAccountIndex();
	bool gameProcess();
	void clickAgreement();
	void inputAccount();
	void inputPasswordAndLogin();
	bool createOneRole();
	BOOL saveVerificationCodeImage();
	BOOL imageMatchFromHwnd(HWND hWnd,const TCHAR* ImagePath,float fSame,
		OUT int& nX,OUT int& nY,bool bSave, bool bGray = true /*是否用灰度图比较*/, int fromX = 0, int fromY = 0);
	IplImage* hBitmapToLpl(HBITMAP hBmp);
	BOOL findImageInGameWnd(const string& image, float fSame = 0.7, bool bGray = true);
	BOOL findImageInLoginWnd(const string& image, float fSame = 0.5);
	string createName(const unsigned int & count);
	BOOL killProcess(const string& processName);
	bool switchVPN();
	Direction getNextStage();
	void FightInAStage();//打一关
	void FightInNextBoss();//打boss临近关卡
	bool BossNext();
	Direction getNextBoss();
	void FightInBoss();//打Boss
	void ScanAndGrabObjects(const int& brick);//扫描和收集物品
	void FightBrick(const int& brick, const Direction& dir);
	bool MoveTowards(const int& brick, const Direction& dir);
	void FightBrickOne();//第一关卡
	void MoveBrickOne();//冲过第一关卡
private:
	HWND m_hShow;
	int	m_Index;
	bool m_Stop;
	int m_RoleIndex;
	CString m_LastIP;
	std::vector<CString> m_vecFiles;
};

