#include "StdAfx.h"
#include "GameControl.h"
#include "KeyMouMng.h"
#include "WaitForEvent.h"
#include "Config.h" 
#include "tlhelp32.h"
#include "VerificationCode.h"
#include "VPNControler.h"
#include "FileParser.h"
#include "MapPosition.h"

char g_ExePath[MAX_PATH] = {0};

CGameControl::CGameControl(HWND hShow):
m_Index(0), m_hShow(hShow),m_Stop(false),m_RoleIndex(0)
{
	GetPath(g_ExePath);
	CFileFind ff;
	CString dir = common::GetModuleDir()+_T("\\MatchImage\\Game\\objects");
	if (dir.Right(1) != "\\")
		dir += "\\";
	dir += "*.*";
	BOOL ret = ff.FindFile(dir);	
	while (ret)
	{
		ret = ff.FindNextFile();
		if (ret != 0)
		{
			if (!ff.IsDirectory() && !ff.IsDots())
			{
				CString name = ff.GetFileName();
				CString path = ff.GetFilePath();
				m_vecFiles.push_back(path);
			}
		}
	}
#ifndef _DEBUG
	killProcess("Client.exe");
	//killProcess("DNF.exe");
#endif
}

bool CGameControl::findCurrentAccountIndex()
{

	killProcess("Client.exe");
	killProcess("DNF.exe");
	if(m_Stop){
		return false;
	}
	//找到第一个没有创建角色的账号
	m_Index++;
	if(m_Index == config_instance.accounts.size())
	{
		return false;
	}
	return true;
}

void CGameControl::Stop()
{
	m_Stop = true;
}

bool CGameControl::gameProcess()
{
	Sleep(1000);
	bool bSuccess = false;
	int tryTimes = 0;
	while(tryTimes++ <= config_instance.ip_try_times){
		if (!StartGame())
		{
			break;
		}
		if(!InputCodes()){
			break;
		}
		auto now = GetTickCount();
		while(true){
			Sleep(500);
			if(GetGameWnd()){
				bSuccess = true;
				break;
			}else if(GetTickCount()-now > 2*60*1000 ){
				break;
			}
		}
		if(bSuccess){
			break;
		}
		killProcess("Client.exe");
		killProcess("DNF.exe");
	}
	LOG_DEBUG<<"登录完成，登录结果 "<<bSuccess;
	if(!bSuccess){
		return findCurrentAccountIndex();
	}
	if(!CreateRole()){
		return findCurrentAccountIndex();;
	}
	EndGame();
	return findCurrentAccountIndex();
}

CGameControl::~CGameControl(void)
{
}

void CGameControl::SelectAreaProcess()
{
	while(!findImageInLoginWnd("SelectArea.png")){
		Sleep(500);
	}
	while(findImageInLoginWnd("SelectArea.png")){
		CKeyMouMng::Ptr()->MouseMoveAndClick(414,549);  //点击选择服务器
		Sleep(500);
	}
	LOG_DEBUG<<"开始选区";
	SelectArea();
	LOG_DEBUG<<"选区完成";
	while(!findImageInLoginWnd("QQ.png")){
		CKeyMouMng::Ptr()->MouseMoveAndClick(1041,554);  //点击登录游戏
		int Times(0);
		bool delayShow = false;
		while( Times++ <= 5){
			if(findImageInLoginWnd("Delay.png")){
				delayShow = true;
				break;
			}
			Sleep(100);
		}
		if(delayShow){
			CKeyMouMng::Ptr()->MouseMoveAndClick(619+rand()%81, 426+rand()%24);  //点击区名
			Sleep(500);
		}
		Sleep(500);
	}
	
}

bool CGameControl::StartGame()
{
#ifndef DEBUG
	if(!switchVPN()){
		PostMessage(m_hShow, WM_UPDATE_GAME_STATUS, (WPARAM)GAME_IP_FAILED, NULL);
		return false;
	}
#endif // DEBUG
	STARTUPINFOA StartupInfo;
	PROCESS_INFORMATION ProcessInformation;
	ZeroMemory(&StartupInfo, sizeof(StartupInfo));
	ZeroMemory(&ProcessInformation, sizeof(ProcessInformation));
	//StartupInfo.dwFlags = STARTF_USESHOWWINDOW;//指定wShowWindow成员有效
	StartupInfo.wShowWindow = TRUE;//此成员设为TRUE的话则显示新建进程的主窗口
	CreateProcessA((config_instance.game_path+"\\Client.exe").c_str(),
		NULL,
		NULL, NULL,
		0,
		NULL,
		NULL,
		config_instance.game_path.c_str(),
		&StartupInfo,
		&ProcessInformation);
	if(ProcessInformation.hProcess==NULL)
	{
		AfxMessageBox(_T("启动游戏失败"), MB_OK);
		return false;
	}
	PostMessage(m_hShow, WM_UPDATE_GAME_STATUS, (WPARAM)GAME_START, NULL);
	return true;
}

bool CGameControl::InputCodes()
{
	SelectAreaProcess();
	PostMessage(m_hShow, WM_UPDATE_GAME_STATUS, (WPARAM)GAME_LOGIN, NULL);
	while (!findImageInLoginWnd("LoginByCode.png"))
	{
		Sleep(500);
	}
	clickAgreement();
	Sleep(500);
	inputAccount();	
	//登录完成，进入验证码界面
	bool bVerficationCode = false;
	int Times = 0;
	while(Times++<=6){
		Sleep(500);
		if(findImageInLoginWnd("VerificationCode.png")||findImageInLoginWnd("PassWordWrong.png")){
			bVerficationCode = true;
			break;
		}
	}
	if(bVerficationCode){
		LOG_DEBUG<<"需要输入验证码";
		int iTryTimes = 0;
		bool bSuccess = false;
		while(iTryTimes<=config_instance.loginFailTimes){
			int verficationCodeTimes = 0;//验证码重试4次
			while(findImageInLoginWnd("VerificationCode.png")&&verficationCodeTimes++<=4){
				if(saveVerificationCodeImage())
				{
					CString strRe = CVerificationCode::Ptr()->pRecYZM_A((LPSTR)(LPCSTR)common::stringToCString(string(g_ExePath)+"VerificationCode\\tmp.png"),
						common::stringToCString(config_instance.verification_account_code),common::stringToCString(config_instance.verification_password),(LPSTR)(LPCSTR)_T("65395"));
					int pos = strRe.Find("|!|");
					if (pos>-1)
					{
						CString result = strRe.Left(pos);
						CKeyMouMng::Ptr()->MouseMoveAndClick(479,365);  //点击验证码编辑框
						CKeyMouMng::Ptr()->InputString(common::CStringTostring(result));
						CKeyMouMng::Ptr()->KeyboardButtonEx(VK_RETURN);
						Sleep(500);
						CKeyMouMng::Ptr()->MouseMoveAndClick(550,457);  //点击验证码确定
						Sleep(500);
					}
				}
			}
			if(verficationCodeTimes==4){
				break;
			}
			Sleep(500);
			//如果没有密码错误的图片，则认为密码输入对了，返回
			if(findImageInLoginWnd("PassWordWrong.png")){
				CKeyMouMng::Ptr()->MouseMoveAndClick(600,434);  //点击确认
				Sleep(500);
				iTryTimes++;
				inputPasswordAndLogin();
			}else{
				return true;
			}
		}
		if(iTryTimes==config_instance.loginFailTimes){
			config_instance.accounts.at(this->m_Index).accountStatus = STATUS_LOGIN_FAILED;
			return false;
		}
	}
	return true;
}

bool CGameControl::CreateRole()
{
	LOG_DEBUG<<"开始创建角色";
	m_RoleIndex = 0;
	//如果已经有毛线团的直接进入创建角色界面，也会跳过
	while (!findImageInGameWnd("GameStart.png")&&!findImageInGameWnd("Role.png")&&!findImageInGameWnd("RiskGroup.png"))
	{
		Sleep(500);
	}
	LOG_DEBUG<<"是否需要输入冒险团";
	while(findImageInGameWnd("RiskGroup.png"))
	{
		CKeyMouMng::Ptr()->MouseMoveAndClickGameWnd(405,290);  //点击冒险团编辑框
		for (auto i = 0; i<20; i++)
		{	
			Sleep(100);
			CKeyMouMng::Ptr()->DirKeyDown(VK_BACK);
			Sleep(100);
			CKeyMouMng::Ptr()->DirKeyUp(VK_BACK);
		}
		const auto groupName = createName(16);
		LOG_DEBUG<<"【冒险团名字】"<<groupName.c_str();
		CKeyMouMng::Ptr()->InputString(groupName);
		CKeyMouMng::Ptr()->KeyboardButtonEx(VK_RETURN);
		CKeyMouMng::Ptr()->MouseMoveAndClickGameWnd(347,428);  //点击设置
		Sleep(2000);
		CKeyMouMng::Ptr()->MouseMoveAndClickGameWnd(387,363);  //点击确定
		Sleep(2000);
		CKeyMouMng::Ptr()->MouseMoveAndClickGameWnd(403,323);  //点击已经设置冒险团名称
	}
	if (findImageInGameWnd("Blocked.png"))
	{
		config_instance.accounts.at(this->m_Index).accountStatus = STATUS_EXCEPTION;
		killProcess("DNF.exe");
		Sleep(1000);
		return false;
	}
	PostMessage(m_hShow, WM_UPDATE_GAME_STATUS, (WPARAM)GAME_CREATE_ROLE, NULL);
	createOneRole();
	if(config_instance.secondRole.compare("不创建角色")!=0){
		createOneRole();
	}
	PostMessage(m_hShow, WM_UPDATE_GAME_STATUS, (WPARAM)GAME_CREATE_ROLE_DONE, NULL);
	return true;
}

void CGameControl::EndGame()
{
	Sleep(1000);
	CKeyMouMng::Ptr()->MouseMoveAndClickGameWnd(550,548);  //点击结束游戏
	Sleep(1000);
	killProcess("DNF.exe");
	Sleep(2000);
}

void CGameControl::FindRolePos()
{
	int nXtmp = 0,nYtmp = 0;
	HWND hGame = GetGameWnd();
	if (hGame == NULL) 
		return ;
	TCHAR ImagePath[MAX_PATH] = {0};
	wsprintf(ImagePath, _T("%sMatchImage\\Game\\Level.png"), g_ExePath);
	imageMatchFromHwnd(hGame,ImagePath,0.5,nXtmp,nYtmp,false);
	LOG_DEBUG<<"查找等级图片 结果 X "<< nXtmp <<" y "<< nYtmp;
	return;
}

void CGameControl::Fight()
{
	CKeyMouMng::Ptr()->MouseMoveAndClickGameWnd(50,50);  //点击冒险团编辑框
	HWND hGame = GetGameWnd();
	if (hGame == NULL) 
		return;
	TCHAR ImagePath[MAX_PATH] = {0};
	posInfo posNext;
	while(true){
		if(BossNext()){
			FightInNextBoss();
			auto next = getNextBoss();
			TRACE(_T("移动到BOSS"));
			LOG_DEBUG<<"BOSS 方向 "<<next;
			if(Direction_DOWN == next){//如果是向下的，那就按向下键和左右
				while(true){
					wsprintf(ImagePath, _T("%sMatchImage\\Game\\Current.png"), g_ExePath);
					imageMatchFromHwnd(hGame,ImagePath,0.6,posNext.posX, posNext.posY,false);
					if(posNext.posX<=700 || posNext.posX >= 800 || posNext.posY<=27 || posNext.posY>= 87){
						break;
					}
					CKeyMouMng::Ptr()->DirKeyDown(VK_DOWN);
					Sleep(2000);
					CKeyMouMng::Ptr()->DirKeyDown(VK_LEFT);
					Sleep(6000);
					CKeyMouMng::Ptr()->DirKeyUp(VK_LEFT);
					Sleep(200);
					CKeyMouMng::Ptr()->DirKeyDown(VK_RIGHT);
					Sleep(6000);
					CKeyMouMng::Ptr()->DirKeyUp(VK_RIGHT);
				}
				CKeyMouMng::Ptr()->DirKeyUp(VK_DOWN);
			}if(Direction_RIGHT == next){//如果是向下的，那就按向下键和左右
				while(true){
					wsprintf(ImagePath, _T("%sMatchImage\\Game\\Current.png"), g_ExePath);
					imageMatchFromHwnd(hGame,ImagePath,0.6,posNext.posX, posNext.posY,false);
					if(posNext.posX<=700 || posNext.posX >= 800 || posNext.posY<=27 || posNext.posY>= 143){
						break;
					}
					CKeyMouMng::Ptr()->DirKeyDown(VK_RIGHT);
					Sleep(2000);
					CKeyMouMng::Ptr()->DirKeyDown(VK_UP);
					Sleep(5000);
					CKeyMouMng::Ptr()->DirKeyUp(VK_UP);
					Sleep(200);
					CKeyMouMng::Ptr()->DirKeyDown(VK_DOWN);
					Sleep(5000);
					CKeyMouMng::Ptr()->DirKeyUp(VK_DOWN);
				}
				CKeyMouMng::Ptr()->DirKeyUp(VK_RIGHT);
			}if(Direction_UP == next){//如果是向下的，那就按向下键和左右
				while(true){
					wsprintf(ImagePath, _T("%sMatchImage\\Game\\Current.png"), g_ExePath);
					imageMatchFromHwnd(hGame,ImagePath,0.6,posNext.posX, posNext.posY,false);
					if(posNext.posX<=700 || posNext.posX >= 800 || posNext.posY<=27 || posNext.posY>= 143){
						break;
					}
					CKeyMouMng::Ptr()->DirKeyDown(VK_UP);
					Sleep(2000);
					CKeyMouMng::Ptr()->DirKeyDown(VK_LEFT);
					Sleep(6000);
					CKeyMouMng::Ptr()->DirKeyUp(VK_LEFT);
					Sleep(200);
					CKeyMouMng::Ptr()->DirKeyDown(VK_RIGHT);
					Sleep(6000);
					CKeyMouMng::Ptr()->DirKeyUp(VK_RIGHT);
				}
				CKeyMouMng::Ptr()->DirKeyUp(VK_UP);
			}if(Direction_LEFT == next){//如果是向下的，那就按向下键和左右
				while(true){
					wsprintf(ImagePath, _T("%sMatchImage\\Game\\Current.png"), g_ExePath);
					imageMatchFromHwnd(hGame,ImagePath,0.6,posNext.posX, posNext.posY,false);
					if(posNext.posX<=700 || posNext.posX >= 800 || posNext.posY<=27 || posNext.posY>= 143){
						break;
					}
					CKeyMouMng::Ptr()->DirKeyDown(VK_LEFT);
					Sleep(2000);
					CKeyMouMng::Ptr()->DirKeyDown(VK_UP);
					Sleep(6000);
					CKeyMouMng::Ptr()->DirKeyUp(VK_UP);
					Sleep(200);
					CKeyMouMng::Ptr()->DirKeyDown(VK_DOWN);
					Sleep(6000);
					CKeyMouMng::Ptr()->DirKeyUp(VK_DOWN);
				}
				CKeyMouMng::Ptr()->DirKeyUp(VK_LEFT);
			}
			FightInBoss();
		}
		else{
		TRACE(_T("一个关卡里面战斗"));
		FightInAStage();
		//如果boss关卡在下一关，则移动到boss
		auto next = getNextStage();
		TRACE(_T("下一个关卡"));
		if(Direction_DOWN == next){//如果是向下的，那就按向下键和左右
			while(true){
			wsprintf(ImagePath, _T("%sMatchImage\\Game\\Next.png"), g_ExePath);
			imageMatchFromHwnd(hGame,ImagePath,0.7,posNext.posX, posNext.posY,false, true);
			if(posNext.posX<=700 || posNext.posX >= 800 || posNext.posY<=27 || posNext.posY>= 124){
				break;
			}
			CKeyMouMng::Ptr()->DirKeyDown(VK_DOWN);
			Sleep(2000);
			CKeyMouMng::Ptr()->DirKeyDown(VK_LEFT);
			Sleep(6000);
			CKeyMouMng::Ptr()->DirKeyUp(VK_LEFT);
			Sleep(200);
			CKeyMouMng::Ptr()->DirKeyDown(VK_RIGHT);
			Sleep(6000);
			CKeyMouMng::Ptr()->DirKeyUp(VK_RIGHT);
			}
			CKeyMouMng::Ptr()->DirKeyUp(VK_DOWN);
		}if(Direction_RIGHT == next){//如果是向下的，那就按向下键和左右
			while(true){
				wsprintf(ImagePath, _T("%sMatchImage\\Game\\Next.png"), g_ExePath);
				imageMatchFromHwnd(hGame,ImagePath,0.7,posNext.posX, posNext.posY,false, true);
				if(posNext.posX<=700 || posNext.posX >= 800 || posNext.posY<=27 || posNext.posY>= 124){
					break;
				}
				CKeyMouMng::Ptr()->DirKeyDown(VK_RIGHT);
				Sleep(2000);
				CKeyMouMng::Ptr()->DirKeyDown(VK_UP);
				Sleep(5000);
				CKeyMouMng::Ptr()->DirKeyUp(VK_UP);
				Sleep(200);
				CKeyMouMng::Ptr()->DirKeyDown(VK_DOWN);
				Sleep(5000);
				CKeyMouMng::Ptr()->DirKeyUp(VK_DOWN);
			}
			CKeyMouMng::Ptr()->DirKeyUp(VK_RIGHT);
		}if(Direction_UP == next){//如果是向下的，那就按向下键和左右
			while(true){
				wsprintf(ImagePath, _T("%sMatchImage\\Game\\Next.png"), g_ExePath);
				imageMatchFromHwnd(hGame,ImagePath,0.7,posNext.posX, posNext.posY,false, true);
				if(posNext.posX<=700 || posNext.posX >= 800 || posNext.posY<=27 || posNext.posY>= 124){
					break;
				}
				CKeyMouMng::Ptr()->DirKeyDown(VK_UP);
				Sleep(2000);
				CKeyMouMng::Ptr()->DirKeyDown(VK_LEFT);
				Sleep(6000);
				CKeyMouMng::Ptr()->DirKeyUp(VK_LEFT);
				Sleep(200);
				CKeyMouMng::Ptr()->DirKeyDown(VK_RIGHT);
				Sleep(6000);
				CKeyMouMng::Ptr()->DirKeyUp(VK_RIGHT);
			}
			CKeyMouMng::Ptr()->DirKeyUp(VK_UP);
		}if(Direction_LEFT == next){//如果是向下的，那就按向下键和左右
			while(true){
				wsprintf(ImagePath, _T("%sMatchImage\\Game\\Next.png"), g_ExePath);
				imageMatchFromHwnd(hGame,ImagePath,0.7,posNext.posX, posNext.posY,false, true);
				if(posNext.posX<=700 || posNext.posX >= 800 || posNext.posY<=27 || posNext.posY>= 124){
					break;
				}
				CKeyMouMng::Ptr()->DirKeyDown(VK_LEFT);
				Sleep(2000);
				CKeyMouMng::Ptr()->DirKeyDown(VK_UP);
				Sleep(5000);
				CKeyMouMng::Ptr()->DirKeyUp(VK_UP);
				Sleep(200);
				CKeyMouMng::Ptr()->DirKeyDown(VK_DOWN);
				Sleep(5000);
				CKeyMouMng::Ptr()->DirKeyUp(VK_DOWN);
			}
			CKeyMouMng::Ptr()->DirKeyUp(VK_LEFT);
		}
		}
	}
}

void CGameControl::FightLinDong()
{
	CKeyMouMng::Ptr()->MouseMoveAndClickGameWnd(10,10);
	while(true){
		Sleep(500);
		CKeyMouMng::Ptr()->DirKeyDown(VK_LEFT);
		while(!findImageInGameWnd("Select.png")){
			Sleep(500);
		}
		Sleep(1000);
		CKeyMouMng::Ptr()->DirKeyUp(VK_LEFT);
		while(!findImageInGameWnd("LinDong.png", 0.5)){
			CKeyMouMng::Ptr()->MouseMoveAndClickGameWnd(315,465);
			Sleep(500);
		}
		while(!findImageInGameWnd("YongShi.png", 0.8, false)){
			if(findImageInGameWnd("Right.png", 0.6, false)){
				CKeyMouMng::Ptr()->DirKeyDown(VK_RIGHT);
				Sleep(100);
				CKeyMouMng::Ptr()->DirKeyUp(VK_RIGHT);
				Sleep(500);

			}else if(findImageInGameWnd("Left.png", 0.6, false)){
				CKeyMouMng::Ptr()->DirKeyDown(VK_RIGHT);
				Sleep(100);
				CKeyMouMng::Ptr()->DirKeyUp(VK_RIGHT);
				Sleep(500);
			}
		}
		CKeyMouMng::Ptr()->MouseMoveAndClickGameWnd(315,465);
		Sleep(2000);
		FightBrickOne();
		continue;
		//定义副本的门口方向
		Direction dirs[] = {Direction_LEFT, Direction_DOWN, Direction_LEFT, Direction_DOWN, Direction_DOWN, Direction_RIGHT, Direction_RIGHT, Direction_RIGHT};
		for(auto i(0); i < 8; i++)
		{
			FightBrick( i, dirs[i]);
			CString text;
			text.Format(_T("战斗完成 ,关卡 %d, 方向 %d \n"), i, dirs[i]);
			ScanAndGrabObjects(i);
			TRACE(text);
			if (!MoveTowards( i, dirs[i]))
			{
				text.Format(_T("关卡移动失败 ,关卡 %d"), i);
				TRACE(text);
				i--;
			}
			text.Format(_T("移动完成 ,关卡 %d"), i);
			TRACE(text);
			if(findImageInGameWnd("End.png")){
				CKeyMouMng::Ptr()->MouseMoveAndClickGameWnd(668,148);
				break;
			}
		}
		if(findImageInGameWnd("ShiKongZhiMen.png")){
			continue;
		}else{
			FightInBoss();
		}
		if(findImageInGameWnd("End.png")){
			CKeyMouMng::Ptr()->MouseMoveAndClickGameWnd(668,148);		
		}
	}
}

void CGameControl::setAccountIndex(const int& index)
{
	this->m_Index = index;
}


void CGameControl::clickAgreement()
{
	if(findImageInLoginWnd("Agreement.png", 0.9)){
		CKeyMouMng::Ptr()->MouseMoveAndClick(948,413); //点击同意协议
		Sleep(300);
	}
	Sleep(100);
}


void CGameControl::inputAccount()
{
	CKeyMouMng::Ptr()->MouseMoveAndClick(1092,328);  //点击编辑框
	Sleep(200);
	for (auto i = 0; i<20; i++)
	{	
		WAIT_STOP_RETURN(10);
		CKeyMouMng::Ptr()->DirKeyDown(VK_BACK);
		Sleep(100);
		CKeyMouMng::Ptr()->DirKeyUp(VK_BACK);
	}
	const auto& account = config_instance.accounts.at(this->m_Index);
	//先输入用户名
	CKeyMouMng::Ptr()->InputString(account.qq);
	CKeyMouMng::Ptr()->KeyboardButtonEx(VK_TAB);
	LOG_DEBUG<<"账号 "<<account.qq.c_str()<<" 密码 "<<account.password.c_str();
	inputPasswordAndLogin();
}

void CGameControl::inputPasswordAndLogin()
{
	Sleep(1000);
	const auto& account = config_instance.accounts.at(this->m_Index);
	CKeyMouMng::Ptr()->InputString(account.password);
	Sleep(1000);
	CKeyMouMng::Ptr()->MouseMoveAndClick(1044,482);  //点击登录
}

bool CGameControl::createOneRole()
{
	LOG_DEBUG<<"可以创建角色了";
	if(!findImageInGameWnd("Role.png")){
		Sleep(1000);
		CKeyMouMng::Ptr()->MouseMoveAndClickGameWnd(178,548);  //点击创建角色
		Sleep(1000);
	}
	SelectProfession();
	m_RoleIndex++;
	CKeyMouMng::Ptr()->MouseMoveAndClickGameWnd(691, 432);  //创建角色第二步
	Sleep(1000);
	while(findImageInGameWnd("RoleType.png"))
	{
		CKeyMouMng::Ptr()->MouseMoveAndClickGameWnd(300, 325);  //创建角色第点击角色类型
		Sleep(1000);
	}
	Sleep(1000);
	while(!findImageInGameWnd("NameCheckPass.png", 0.99, false))
	{
		for (auto i = 0; i<12; i++)
		{	
			Sleep(100);
			CKeyMouMng::Ptr()->DirKeyDown(VK_BACK);
			Sleep(100);
			CKeyMouMng::Ptr()->DirKeyUp(VK_BACK);
		}
		const auto name = createName(12);
		auto& account = config_instance.accounts.at(this->m_Index);
		account.role_name.push_back(name);
		config_instance.SaveData();
		LOG_DEBUG<<"【角色名字】"<<name.c_str();
		CKeyMouMng::Ptr()->InputString(name);
		CKeyMouMng::Ptr()->KeyboardButtonEx(VK_RETURN);
		CKeyMouMng::Ptr()->MouseMoveAndClickGameWnd(479,289);  //点击检测重复
		LOG_DEBUG<<"点击检测重复 "<<name.c_str();
		Sleep(1000);
		CKeyMouMng::Ptr()->MouseMoveAndClickGameWnd(400,321);//点击检测结果
		Sleep(1000);
	}
	while(findImageInGameWnd("Role.png"))
	{
		CKeyMouMng::Ptr()->MouseMoveAndClickGameWnd(370,354); //点击确定
		Sleep(500);
		CKeyMouMng::Ptr()->MouseMoveAndClickGameWnd(373,320); //点击确定
		Sleep(500);
	}
	LOG_DEBUG<<"点击确定";
	Sleep(1000);

	int nXtmp = 0,nYtmp = 0;
	HWND hGameWnd = GetGameWnd();
	if (hGameWnd == NULL) 
		return false;
	TCHAR ImagePath[MAX_PATH] = {0};
	wsprintf(ImagePath, _T("%sMatchImage\\Game\\Success.png"), g_ExePath);
	auto bFind = imageMatchFromHwnd(hGameWnd,ImagePath,0.5,nXtmp,nYtmp,false);
	if (bFind)
	{
		LOG_DEBUG<<"成功 的坐标 "<<nXtmp<<" y "<< nYtmp;
		CKeyMouMng::Ptr()->MouseMoveAndClickGameWnd(nXtmp+95, nYtmp+76);  //点击编辑框
		config_instance.accounts.at(this->m_Index).accountStatus = STATUS_SUCCESS;
	}
	Sleep(500);
	LOG_DEBUG<<"点击创建角色成功";
	return true;
}

BOOL CGameControl::saveVerificationCodeImage()
{
	auto hWnd = GetLoginWnd();
	BOOL bresMatched = FALSE;
	LPBYTE   lpBits;
	IplImage    *Img = NULL;
	//获得桌面句柄
	try
	{	
		HWND hDesktop = ::GetDesktopWindow();  
		//如果句柄为空则接取全屏来找
		if(NULL == hWnd)  
		{  
			hWnd = hDesktop;
		}  

		RECT rect;  
		::GetWindowRect(hWnd, &rect);  

		if (rect.left < 0 || rect.top < 0)
		{
			LOG_DEBUG<<"窗体被最小化了";
			SetWindowPos(hWnd,HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE);
			::GetWindowRect(hWnd, &rect); 
		}else
		{
			SetWindowPos(hWnd,HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_DRAWFRAME | SWP_NOSIZE);
		}

		int nWidth = rect.right - rect.left;  
		int nHeight = rect.bottom - rect.top;  

		int size = sizeof(BITMAPINFO) + 255 * sizeof(RGBQUAD);
		BITMAPINFO* bmpinfo = (BITMAPINFO*)(new BYTE[size]);// 因为BITMAPINFO结构中已经包含了一个RGBQUAD成员，所以分配时对于256色只需再跟255个RGBQUAD的空间
		ZeroMemory(bmpinfo, size);
		bmpinfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmpinfo->bmiHeader.biWidth = nWidth;
		bmpinfo->bmiHeader.biHeight = nHeight;
		bmpinfo->bmiHeader.biPlanes = 1;
		bmpinfo->bmiHeader.biBitCount = 32;
		bmpinfo->bmiHeader.biCompression = BI_RGB;
		//bmpinfo->bmiHeader.biClrUsed = 0; // 前面都已经初始化为0，不用重复
		//bmpinfo->bmiHeader.biClrImportant = 0;
		bmpinfo->bmiHeader.biSizeImage = nWidth*nHeight*4;// 对于biCompression设为BI_RGB的此参数可为0

		BITMAPINFO* tmpBmp = new BITMAPINFO(*bmpinfo);
		tmpBmp->bmiHeader.biWidth = 118;
		tmpBmp->bmiHeader.biHeight = 39;
		HDC hSrcDC = ::GetWindowDC(hWnd);  
		HDC hMemDC = ::CreateCompatibleDC(hSrcDC); 
		HBITMAP hBitmap = CreateDIBSection(hSrcDC, tmpBmp, DIB_RGB_COLORS,(void **)&lpBits, NULL, 0);
		//HBITMAP hBitmap = ::CreateCompatibleBitmap(hSrcDC, nWidht, nHeight);  
		HBITMAP hOldBitmap = (HBITMAP)::SelectObject(hMemDC, hBitmap);
		BOOL bRet = ::BitBlt(hMemDC, 0, 0, 118, 39, hSrcDC, 532, 344, SRCCOPY|CAPTUREBLT); 
		//_DbgPrint("bRet = %d, 宽 = %d, 高 = %d, 窗体X坐标 = %d, 窗体Y坐标= %d",
		//		   bRet ,nWidht,nHeight,rect.left,rect.top);

		Img = hBitmapToLpl(hBitmap);

		if (Img == NULL)
		{
			cvReleaseImage(&Img);
			goto _Error;
		}
		cvSaveImage((string(g_ExePath)+"VerificationCode\\tmp.png").c_str(),Img);
		//-----------------------------
_Error:
		if(Img){
			cvReleaseImage(&Img);
		}	

		::SelectObject(hMemDC, hOldBitmap);  
		::DeleteObject(hBitmap);  
		::DeleteDC(hMemDC);  
		::ReleaseDC(hWnd, hSrcDC);  
		delete [] bmpinfo;
		bmpinfo = NULL;
		delete [] tmpBmp;
		tmpBmp = NULL;
		SetWindowPos(hWnd,HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_DRAWFRAME | SWP_NOSIZE);
	}catch(...)
	{
		LPVOID lpMsgBuf;
		FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			GetLastError(),
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
			(LPTSTR) &lpMsgBuf,
			0,
			NULL
			);

		LOG_DEBUG<<(char*)lpMsgBuf;

		LocalFree(lpMsgBuf);
		return FALSE;
	}
	return TRUE;
}

BOOL CGameControl::imageMatchFromHwnd(HWND hWnd,const TCHAR* ImagePath,float fSame, OUT int& nX,OUT int& nY,bool bSave, bool bGray, int fromX, int fromY)
{
	BOOL bresMatched = FALSE;
	LPBYTE   lpBits;
	IplImage    *Img = NULL;
	IplImage    *ImgGray = NULL;
	IplImage    *Tpl = NULL;
	IplImage    *TplGray = NULL;
	IplImage    *matRes = NULL;
	IplImage    *matchArea = NULL;
	IplImage    *ImgCompare = NULL;
	IplImage    *TplCompare = NULL;
	double min_val;
	double max_val;
	CvPoint min_loc;
	CvPoint max_loc;
	nX = 0;
	nY = 0;
	//获得桌面句柄
	try
	{	
		HWND hDesktop = ::GetDesktopWindow();  
		//如果句柄为空则接取全屏来找
		if(NULL == hWnd)  
		{  
			hWnd = hDesktop;
		}  

		RECT rect;  
		::GetWindowRect(hWnd, &rect);  

		if (rect.left < 0 || rect.top < 0)
		{
			LOG_DEBUG<<"窗体被最小化了";
			SetWindowPos(hWnd,HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE);
			::GetWindowRect(hWnd, &rect); 
		}else
		{
			SetWindowPos(hWnd,HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_DRAWFRAME | SWP_NOSIZE);
		}

		int nWidth = rect.right - rect.left;  
		int nHeight = rect.bottom - rect.top;  
		if(nWidth>fromX&&fromX!=0){
			nWidth -= fromX;
		}
		if(nHeight>fromY&&fromY!=0){
			nHeight -= fromY;
		}

		int size = sizeof(BITMAPINFO) + 255 * sizeof(RGBQUAD);
		BITMAPINFO* bmpinfo = (BITMAPINFO*)(new BYTE[size]);// 因为BITMAPINFO结构中已经包含了一个RGBQUAD成员，所以分配时对于256色只需再跟255个RGBQUAD的空间
		ZeroMemory(bmpinfo, size);
		bmpinfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmpinfo->bmiHeader.biWidth = nWidth;
		bmpinfo->bmiHeader.biHeight = nHeight;
		bmpinfo->bmiHeader.biPlanes = 1;
		bmpinfo->bmiHeader.biBitCount = 32;
		bmpinfo->bmiHeader.biCompression = BI_RGB;
		//bmpinfo->bmiHeader.biClrUsed = 0; // 前面都已经初始化为0，不用重复
		//bmpinfo->bmiHeader.biClrImportant = 0;
		bmpinfo->bmiHeader.biSizeImage = nWidth*nHeight*4;// 对于biCompression设为BI_RGB的此参数可为0

		HDC hSrcDC = ::GetWindowDC(hWnd);  
		HDC hMemDC = ::CreateCompatibleDC(hSrcDC); 
		HBITMAP hBitmap = CreateDIBSection(hSrcDC, bmpinfo, DIB_RGB_COLORS,(void **)&lpBits, NULL, 0);
		//HBITMAP hBitmap = ::CreateCompatibleBitmap(hSrcDC, nWidht, nHeight);  
		HBITMAP hOldBitmap = (HBITMAP)::SelectObject(hMemDC, hBitmap);  
		BOOL bRet = ::BitBlt(hMemDC, 0, 0, nWidth, nHeight, hSrcDC, fromX, fromY, SRCCOPY|CAPTUREBLT); 
		//_DbgPrint("bRet = %d, 宽 = %d, 高 = %d, 窗体X坐标 = %d, 窗体Y坐标= %d",
		//		   bRet ,nWidht,nHeight,rect.left,rect.top);

		Img = hBitmapToLpl(hBitmap);

		if (Img == NULL)
		{
			cvReleaseImage(&Img);
			goto _Error;
		}

		ImgGray = cvCreateImage(cvGetSize(Img),8,1);
		cvCvtColor(Img,ImgGray,CV_BGR2GRAY);

		if (bSave)
		{
			cvSaveImage("E:\\tmpfile\\Img.png",Img);
			cvSaveImage("E:\\tmpfile\\ImgGray.png",ImgGray);
		}

		Tpl = cvLoadImage( ImagePath, CV_LOAD_IMAGE_COLOR );

		if( Tpl == NULL ) 
		{ 
			LOG_DEBUG<<"打开原图像失败:"<<ImagePath;
			goto _Error;
		} 

		TplGray = cvCreateImage(cvGetSize(Tpl),8,1);
		cvCvtColor(Tpl,TplGray,CV_BGR2GRAY);

		if (bSave)
		{
			cvSaveImage("E:\\tmpfile\\Tpl.png",Tpl);
			cvSaveImage("E:\\tmpfile\\TplGray.png",TplGray);
		}
		//---------------------开始比对
		int iwidth = Img->width - Tpl->width + 1;
		int iheight = Img->height - Tpl->height + 1;
		//创建匹配结果图
		matRes=cvCreateImage(cvSize(iwidth, iheight),IPL_DEPTH_32F, 1);  
		LOG_DEBUG<<"matRes:"<<matRes;
		//找到匹配位置
		if(bGray){
			ImgCompare = ImgGray;
			TplCompare = TplGray;
		}else{
			ImgCompare = Img;
			TplCompare = Tpl;
		}
		cvMatchTemplate(ImgCompare,TplCompare,matRes,CV_TM_CCOEFF_NORMED);
		//找到最佳匹配位置 
		cvMinMaxLoc(matRes,&min_val,&max_val,&min_loc,&max_loc,NULL);
		//_DbgPrint("匹配位置X = %d,Y = %d",max_loc.x,max_loc.y);

		/*cvSetImageROI(ImgCompare,cvRect(max_loc.x,max_loc.y,TplCompare->width,TplCompare->height));
		matchArea = cvCreateImage(cvGetSize(TplCompare),8,1);
		cvCopy(ImgCompare,matchArea,0);
		cvResetImageROI(ImgCompare);

		if (bSave)
		{
		cvSaveImage("E:\\tmpfile\\matchedArea.png",matchArea);
		}*/

		bresMatched =(fSame <= max_val)?TRUE:FALSE;

		if (bresMatched)
		{
			//匹配上了
			//_DbgPrint("匹配成功");
			nX = max_loc.x;
			nY = max_loc.y;
		}
		else
		{
			//_DbgPrint("匹配不成功");
		}

		//-----------------------------
_Error:
		if (matRes)
		{
			cvReleaseImage(&matRes);
		}
		if(matchArea){
			cvReleaseImage(&matchArea);
		}
		if(Img){
			cvReleaseImage(&Img);
		}
		if(Tpl){
			cvReleaseImage(&Tpl);
		}
		if(ImgGray){
			cvReleaseImage(&ImgGray);
		}
		if(TplGray){
			cvReleaseImage(&TplGray);
		}

		::SelectObject(hMemDC, hOldBitmap);  
		::DeleteObject(hBitmap);  
		::DeleteDC(hMemDC);  
		::ReleaseDC(hWnd, hSrcDC);  
		delete [] bmpinfo;
		bmpinfo = NULL;
		SetWindowPos(hWnd,HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_DRAWFRAME | SWP_NOSIZE);
	}catch(...)
	{
		LPVOID lpMsgBuf;
		FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			GetLastError(),
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
			(LPTSTR) &lpMsgBuf,
			0,
			NULL
			);

		LOG_DEBUG<<(char*)lpMsgBuf;

		LocalFree(lpMsgBuf);
		return NULL;
	}


	return bresMatched;
}

IplImage* CGameControl::hBitmapToLpl(HBITMAP hBmp)
{   
	int image_width=0;
	int image_height=0;
	int image_depth=0;
	int image_nchannels=0;
	try
	{	
		BITMAP bmp;
		GetObject(hBmp,sizeof(BITMAP),&bmp);   			 	

		image_nchannels = bmp.bmBitsPixel == 1 ? 1 : bmp.bmBitsPixel/8 ;
		image_depth = bmp.bmBitsPixel == 1 ? IPL_DEPTH_1U : IPL_DEPTH_8U;
		image_width=bmp.bmWidth;
		image_height=bmp.bmHeight;
		////_DbgPrint("image_width=%d,image_height=%d,image_depth=%d,image_nchannels=%d",image_width,image_height,image_depth,image_nchannels);
		IplImage* img = cvCreateImage(cvSize(image_width,image_height),image_depth,image_nchannels); 
		BYTE *pBuffer = new BYTE[image_height*image_width*image_nchannels];    	  
		GetBitmapBits(hBmp,image_height*image_width*image_nchannels,pBuffer);    
		memcpy(img->imageData,pBuffer,image_height*image_width*image_nchannels);   
		delete[] pBuffer;    
		IplImage *dst = cvCreateImage(cvGetSize(img),img->depth,3);
		cvCvtColor(img,dst,CV_BGRA2BGR); 
		cvReleaseImage(&img);   
		return dst;   
	}
	catch(...)
	{
		LPVOID lpMsgBuf;
		FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			GetLastError(),
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
			(LPTSTR) &lpMsgBuf,
			0,
			NULL
			);

		LOG_DEBUG<<(char*)lpMsgBuf;

		LocalFree(lpMsgBuf);
		return NULL;
	}
}

BOOL CGameControl::findImageInGameWnd(const string& image, float fSame, bool bGray)
{
	int nXtmp = 0,nYtmp = 0;
	HWND hGameWnd = GetGameWnd();
	if (hGameWnd == NULL) 
		return FALSE;
	TCHAR ImagePath[MAX_PATH] = {0};
	wsprintf(ImagePath, _T("%sMatchImage\\Game\\%s"), g_ExePath, image.c_str());
	auto bFind = imageMatchFromHwnd(hGameWnd,ImagePath,fSame,nXtmp,nYtmp,false, bGray);
	LOG_DEBUG<<"查找图片"<<image.c_str()<< " 结果 "<<bFind<<" X "<< nXtmp <<" y "<< nYtmp;
	return bFind;
}

BOOL CGameControl::findImageInLoginWnd(const string& image,  float fSame)
{
	int nXtmp = 0,nYtmp = 0;
	HWND hLogin = GetLoginWnd();
	if (hLogin == NULL) 
		return FALSE;
	TCHAR ImagePath[MAX_PATH] = {0};
	wsprintf(ImagePath, _T("%sMatchImage\\Game\\%s"), g_ExePath, image.c_str());
	auto bFind = imageMatchFromHwnd(hLogin,ImagePath,fSame,nXtmp,nYtmp,false);
	LOG_DEBUG<<"查找图片"<<image.c_str()<< " 结果 "<<bFind<<" X "<< nXtmp <<" y "<< nYtmp;
	return bFind;
}

string CGameControl::createName(const unsigned int & count)
{
	string Name;
	srand((int)time(0));
	for(unsigned int i(0); i < count; i++){
		char ch = 'a';
		if(rand()%2==0){
			ch = rand()%26+'a';
		}else{
			ch = rand()%10+'0';
		}
		Name.push_back(ch);
	}
	return Name;
}

void CGameControl::SelectProfession()
{
	srand((int)time(0));
	auto positionX = 0;
	auto positionY = 0;
	auto roleName = config_instance.firstRole;
	auto professionName = config_instance.firstRoleProfession;
	if (m_RoleIndex==1)
	{
		roleName = config_instance.secondRole;
		professionName = config_instance.secondRoleProfession;
	}
	for (auto i(0); i < config_instance.professionPositions.size(); i++)
	{
		if(config_instance.professionPositions.at(i).name == roleName)
		{
			positionX = config_instance.professionPositions.at(i).positionX+10+rand()%53;
			positionY = config_instance.professionPositions.at(i).positionY+10+rand()%43;
			break;
		}
	}
	CKeyMouMng::Ptr()->MouseMoveAndClickGameWnd(positionX, positionY);  //点击第一职业
	Sleep(500+rand()%500);
	auto professionIndex(0);//角色第二职业的个数，用于定位
	for (auto i(0); i < config_instance.professions.size(); i++)
	{
		if(config_instance.professions.at(i).name == roleName)
		{
			for (auto j(0); j < config_instance.professions.at(i).profession.size(); j++)
			{
				if(config_instance.professions.at(i).profession.at(j) == professionName)
				{
					professionIndex = j;
					break;
				}
			}
			break;
		}
	}
	CKeyMouMng::Ptr()->MouseMoveAndClickGameWnd(177+rand()%53+10+professionIndex*77,408+rand()%27+10);  //点击第二职业
	Sleep(500+rand()%500);
}

void CGameControl::SelectArea()
{
	srand((int)time(0));
	auto nm = config_instance.servername;
	for(auto i(0); i < config_instance.game_area.size(); i++)
	{
		const auto& areaInfo = config_instance.game_area.at(i);
		if(areaInfo.name == config_instance.areaname)
		{
			if(areaInfo.group == "Telecom"){
				CKeyMouMng::Ptr()->MouseMoveAndClick(222+rand()%25, 119+rand()%60);  //点击电信
			}else if(areaInfo.group == "Unicom"){
				CKeyMouMng::Ptr()->MouseMoveAndClick(222+rand()%25, 207+rand()%60);  //点击联通
			}
			Sleep(800);
			//根据index来点击
			CKeyMouMng::Ptr()->MouseMoveAndClick(280+rand()%90+(areaInfo.index%5)*123, 148+rand()%25+(areaInfo.index/5)*52);  //点击服务器名字
			Sleep(800);
			for(auto j(0); j < areaInfo.server.size(); j++)
			{
				if(areaInfo.server.at(j) == config_instance.servername)
				{
					CKeyMouMng::Ptr()->MouseMoveAndClick(280+rand()%90+(j%5)*123, 384+rand()%25+(j/5)*52);  //点击区名
					Sleep(800);
					break;
				}
			}
			break;
		}
	}	
}

void CGameControl::outputFile()
{
	CFileParser file;
	file.SaveData();
}

void CGameControl::resetIndex()
{
	m_Stop = false;
	m_Index = 0;
}

void CGameControl::GameLoop()
{
	config_instance.SaveData();
	if(m_Index >= config_instance.accounts.size()){
		return;
	}
	while(gameProcess()){};
	resetIndex();
	outputFile();
}

BOOL CGameControl::killProcess(const string& processName)
{
	CString strProcessName = common::stringToCString(processName);

	strProcessName.MakeLower();

	//创建进程快照(TH32CS_SNAPPROCESS表示创建所有进程的快照)

	HANDLE hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	//PROCESSENTRY32进程快照的结构体
	PROCESSENTRY32 pe;
	//实例化后使用Process32First获取第一个快照的进程前必做的初始化操作
	pe.dwSize = sizeof(PROCESSENTRY32);
	//下面的IF效果同:

	//if(hProcessSnap == INVALID_HANDLE_VALUE)   无效的句柄
	if (!Process32First(hSnapShot, &pe))
	{
		return FALSE;
	}

	//如果句柄有效  则一直获取下一个句柄循环下去
	while (Process32Next(hSnapShot, &pe))
	{
		//pe.szExeFile获取当前进程的可执行文件名称
		CString scTmp = pe.szExeFile;
		//比较当前进程的可执行文件名称和传递进来的文件名称是否相同
		//相同的话Compare返回0
		if (scTmp.MakeLower().Find(strProcessName) != -1)
		{
			//从快照进程中获取该进程的PID(即任务管理器中的PID)
			DWORD dwProcessID = pe.th32ProcessID;
			HANDLE hProcess = ::OpenProcess(PROCESS_TERMINATE, FALSE, dwProcessID);
			::TerminateProcess(hProcess, 0);
			CloseHandle(hProcess);
			return TRUE;
		}
	}
	CloseHandle(hSnapShot);
	return FALSE;
}

bool CGameControl::switchVPN()
{
	if(config_instance.proxy_method == common::CStringTostring(ProxyEt)){
		CVPNControler controler;
		controler.clickOnSwitchButton();
		auto Times(0);
		while(Times++<=config_instance.ip_try_times){
			Sleep(1000);
			CString currentIP = CVPNControler::GetSystemIp();
			LOG_DEBUG<<"ip "<<common::CStringTostring(currentIP).c_str();
			if(currentIP.Compare(common::stringToCString(config_instance.ip_address))!=0&&m_LastIP.Compare(currentIP)!=0)
			{
				m_LastIP = currentIP;
				PostMessage(m_hShow, WM_UPDATE_GAME_STATUS, (WPARAM)GAME_IP, (LPARAM)&m_LastIP);
				return true;
			}
		}
	}else{
		return true;
	}
	return false;
}

Direction CGameControl::getNextStage()
{
	HWND hGame = GetGameWnd();
	if (hGame == NULL) 
		return Direction_NULL;
	TCHAR ImagePath[MAX_PATH] = {0};
	posInfo posNext;
	wsprintf(ImagePath, _T("%sMatchImage\\Game\\Next.png"), g_ExePath);
	imageMatchFromHwnd(hGame,ImagePath,0.5,posNext.posX, posNext.posY,false);
	if(posNext.posX==0&&posNext.posY==0){
		return Direction_NULL;
	}
	posInfo posCurrent;
	wsprintf(ImagePath, _T("%sMatchImage\\Game\\Current.png"), g_ExePath);
	imageMatchFromHwnd(hGame,ImagePath,0.5,posCurrent.posX, posCurrent.posY,false);
	CMapPosition position;
	return position.getNextDirection(posCurrent, posNext);
}

void CGameControl::FightInAStage()
{
	HWND hGame = GetGameWnd();
	if (hGame == NULL) 
		return;
	TCHAR ImagePath[MAX_PATH] = {0};
	posInfo posNext;		
	vector<string> skills;
	skills.push_back("q");
	skills.push_back("xxxxxx");
	skills.push_back("w");
	skills.push_back("e");
	skills.push_back("r");
	skills.push_back("t");
	skills.push_back("s");
	skills.push_back("d");
	skills.push_back("f");
	skills.push_back("g");
	skills.push_back("h");
	CKeyMouMng::Ptr()->RoleMoveRandom(800);
	srand((int)time(0));
	while(true){
		wsprintf(ImagePath, _T("%sMatchImage\\Game\\Next.png"), g_ExePath);
		imageMatchFromHwnd(hGame,ImagePath,0.7,posNext.posX, posNext.posY,false, false);
		if(posNext.posX>=700 &&posNext.posX <=800 && posNext.posY>=27 && posNext.posY<= 124){
			return;
		}
		if(findImageInGameWnd("BossOpen.png")){
			return;
		}
		for (auto count(0); count < 8; count++)
		{
			const string skill(skills.at(rand()%skills.size()));
			for (auto i(0); i < skill.size(); i++)
			{
				CKeyMouMng::Ptr()->KeyboardButtonEx(skill.at(i)-32);
				Sleep(50);
			}
		}
		CKeyMouMng::Ptr()->KeyboardButtonEx('s'-32);
		Sleep(50);
	}
}

void CGameControl::FightInNextBoss()
{
	TRACE(_T("临近BOSS关卡"));
	HWND hGame = GetGameWnd();
	if (hGame == NULL) 
		return;
	TCHAR ImagePath[MAX_PATH] = {0};
	posInfo posNext;		
	vector<string> skills;
	skills.push_back("q");
	skills.push_back("xxxxxx");
	skills.push_back("w");
	skills.push_back("e");
	skills.push_back("r");
	skills.push_back("t");
	skills.push_back("s");
	skills.push_back("d");
	skills.push_back("f");
	skills.push_back("g");
	skills.push_back("h");
	srand((int)time(0));
	while(true){
		wsprintf(ImagePath, _T("%sMatchImage\\Game\\BossOpen.png"), g_ExePath);
		imageMatchFromHwnd(hGame,ImagePath,0.6,posNext.posX, posNext.posY,false);
		if(posNext.posX!=0||posNext.posY!=0){
			return;
		}
		for (auto count(0); count < 8; count++)
		{
			const string skill(skills.at(rand()%skills.size()));
			for (auto i(0); i < skill.size(); i++)
			{
				CKeyMouMng::Ptr()->KeyboardButtonEx(skill.at(i)-32);
				Sleep(50);
			}
		}
		CKeyMouMng::Ptr()->KeyboardButtonEx('s'-32);
		Sleep(100);
	}
}

bool CGameControl::BossNext()
{
	HWND hGame = GetGameWnd();
	if (hGame == NULL) 
		return false;
	TCHAR ImagePath[MAX_PATH] = {0};
	posInfo posBoss;
	wsprintf(ImagePath, _T("%sMatchImage\\Game\\Boss.png"), g_ExePath);
	imageMatchFromHwnd(hGame,ImagePath,0.5,posBoss.posX, posBoss.posY,false);
	
	posInfo posCurrent;
	wsprintf(ImagePath, _T("%sMatchImage\\Game\\Current.png"), g_ExePath);
	imageMatchFromHwnd(hGame,ImagePath,0.5,posCurrent.posX, posCurrent.posY,false);
	if(posBoss.posX - posCurrent.posX <=18 && posBoss.posY - posCurrent.posY <= 10){
		TRACE(_T("BOSS在下一关"));
		return true;
	}
	return false;
}

Direction CGameControl::getNextBoss()
{
	HWND hGame = GetGameWnd();
	if (hGame == NULL) 
		return Direction_NULL;
	TCHAR ImagePath[MAX_PATH] = {0};
	posInfo posNext;
	wsprintf(ImagePath, _T("%sMatchImage\\Game\\Boss.png"), g_ExePath);
	imageMatchFromHwnd(hGame,ImagePath,0.5,posNext.posX, posNext.posY,false);
	if(posNext.posX==0&&posNext.posY==0){
		return Direction_NULL;
	}
	posInfo posCurrent;
	wsprintf(ImagePath, _T("%sMatchImage\\Game\\Current.png"), g_ExePath);
	imageMatchFromHwnd(hGame,ImagePath,0.5,posCurrent.posX, posCurrent.posY,false);
	CMapPosition position;
	return position.getNextDirection(posCurrent, posNext);
}

void CGameControl::FightInBoss()
{	
	TRACE(_T("BOSS关卡\n"));
	vector<string> skills;
	skills.push_back("q");
	skills.push_back("xxxxxx");
	skills.push_back("w");
	skills.push_back("e");
	skills.push_back("r");
	skills.push_back("t");
	skills.push_back("s");
	skills.push_back("d");
	skills.push_back("f");
	skills.push_back("g");
	skills.push_back("h");
	skills.push_back("y");
	CKeyMouMng::Ptr()->RoleMoveRandom(800);
	srand((int)time(0));
	while(true){
		for (auto count(0); count < 8; count++)
		{
			const string skill(skills.at(rand()%skills.size()));
			for (auto i(0); i < skill.size(); i++)
			{
					CKeyMouMng::Ptr()->KeyboardButtonEx(skill.at(i)-32);
					Sleep(50);
			}
		}
		CKeyMouMng::Ptr()->KeyboardButtonEx('s'-32);
		Sleep(100);
		if(findImageInGameWnd("Reward.png", 0.7)){
			break;
		}
	}
}

void CGameControl::ScanAndGrabObjects(const int& brick)
{
	//首先扫描掉落物品
	HWND hGame = GetGameWnd();
	if (hGame == NULL) 
		return;
	posInfo posLevel;
	posInfo posObj;	
	TRACE(_T("开始拾取物品"));
		TCHAR ImageLevel[MAX_PATH] = {0};
	wsprintf(ImageLevel, _T("%sMatchImage\\Game\\Level.png"), g_ExePath);
	auto iTimes(0);
	while(true){
		auto count(0);
		if(brick!=0&&!findImageInGameWnd("Next.png")){
			break;
		}
		for(auto index(0); index < m_vecFiles.size(); index++){
			if(imageMatchFromHwnd(hGame,m_vecFiles.at(index),0.7,posObj.posX, posObj.posY,false, true)){
				count++;
				TRACE(m_vecFiles.at(index)+ _T(" found\n"));
				imageMatchFromHwnd(hGame,ImageLevel,0.5,posLevel.posX, posLevel.posY,false);
				if(posLevel.posX==0){//如果找不到人物的位置，则向右移动
					CKeyMouMng::Ptr()->DirKeyDown(VK_RIGHT);
					Sleep(1000);
					CKeyMouMng::Ptr()->DirKeyUp(VK_RIGHT);
					if(brick!=0&&!findImageInGameWnd("Next.png")){
						break;
					}
					continue;
				}
				posLevel.posY += 120;
				posLevel.posX += 20;
				auto distanceY = posLevel.posY-posObj.posY;
				if( distanceY > 20){
					CKeyMouMng::Ptr()->DirKeyDown(VK_UP);
					Sleep(((float)distanceY)*5.8);
					CKeyMouMng::Ptr()->DirKeyUp(VK_UP);
				}else if(distanceY < -20){
					CKeyMouMng::Ptr()->DirKeyDown(VK_DOWN);
					Sleep(((float)abs(distanceY))*5.8);
					CKeyMouMng::Ptr()->DirKeyUp(VK_DOWN);
				}
				auto distanceX = posLevel.posX-posObj.posX;
				if( distanceX > 20){
					CKeyMouMng::Ptr()->DirKeyDown(VK_LEFT);
					Sleep(((float)distanceX)*5.8);
					CKeyMouMng::Ptr()->DirKeyUp(VK_LEFT);
				}else if(distanceX < -20){
					CKeyMouMng::Ptr()->DirKeyDown(VK_RIGHT);
					Sleep(((float)abs(distanceX))*5.8);
					CKeyMouMng::Ptr()->DirKeyUp(VK_RIGHT);
				}
				CKeyMouMng::Ptr()->KeyboardButtonEx('x'-32);
				TRACE("X pressed");
			}
		}
		CString text;
		text.Format(_T("object count %d"), count);
		TRACE(text);
		if (count==0)
		{
			break;
		}
		if(iTimes++>=5)
		{
			break;
		}
	}
}

void CGameControl::FightBrick(const int& brick, const Direction& dir)
{
	HWND hGame = GetGameWnd();
	if (hGame == NULL) 
		return;
	TCHAR ImagePath[MAX_PATH] = {0};
	posInfo posNext;		
	vector<string> skills;
	skills.push_back("q");
	skills.push_back("xxx");
	skills.push_back("w");
	skills.push_back("e");
	skills.push_back("r");
	skills.push_back("t");
	skills.push_back("s");
	skills.push_back("d");
	skills.push_back("f");
	skills.push_back("g");
	skills.push_back("h");
	//根据门口方向走到左边或者右边5秒
	if(brick==0){
		CKeyMouMng::Ptr()->DirKeyDown(VK_LEFT);
	}
	string dirOpen;
	if(dir==Direction_LEFT){
		dirOpen = "LeftDoor.png";
	}else if(dir==Direction_RIGHT){
		dirOpen = "RightDoor.png";
	}else if(dir==Direction_UP){
		dirOpen = "UpDoor.png";
	}else if(dir==Direction_DOWN){
		dirOpen = "DownDoor.png";
	}


	srand((int)time(0));
	while(true){
		wsprintf(ImagePath, _T("%sMatchImage\\Game\\Next.png"), g_ExePath);
		imageMatchFromHwnd(hGame,ImagePath,0.7,posNext.posX, posNext.posY,false, false);
		if(posNext.posX>=700 &&posNext.posX <=800 && posNext.posY>=27 && posNext.posY<= 124){
			break;
		}
		if(brick==0){
			if(findImageInGameWnd("Brick1Open.png", 0.7)){
				break;
			}
			if(findImageInGameWnd("Brick2.png")){
				break;
			}
		}
		if(findImageInGameWnd("End.png")){
			break;
		}
		for (auto count(0); count < 3; count++)
		{
			const string skill(skills.at(rand()%skills.size()));
			for (auto i(0); i < skill.size(); i++)
			{
				CKeyMouMng::Ptr()->KeyboardButtonEx(skill.at(i)-32);
				Sleep(50);
			}
		}
		CKeyMouMng::Ptr()->KeyboardButtonEx('s'-32);
		Sleep(50);
	}
	if(brick==0){
		CKeyMouMng::Ptr()->DirKeyUp(VK_LEFT);
	}
}

bool CGameControl::MoveTowards(const int& brick, const Direction& dir)
{
	HWND hGame = GetGameWnd();
	if (hGame == NULL) 
		return false;
	TCHAR ImageLevel[MAX_PATH] = {0};
	TCHAR ImageDoor[MAX_PATH] = {0};
	CString passThroughImage;
	passThroughImage.Format(_T("Brick%d.png"), brick+2);
	posInfo posLevel;
	posInfo posDoor;
	if(Direction_LEFT == dir){
			wsprintf(ImageLevel, _T("%sMatchImage\\Game\\Level.png"), g_ExePath);
			if(brick==0){
				wsprintf(ImageDoor, _T("%sMatchImage\\Game\\BrickOneDoor.png"), g_ExePath);
			}else{
				wsprintf(ImageDoor, _T("%sMatchImage\\Game\\LeftDoor.png"), g_ExePath);
			}
			int iTimes = 0;
			while(true){
				imageMatchFromHwnd(hGame,ImageLevel,0.6,posLevel.posX, posLevel.posY,false);
				imageMatchFromHwnd(hGame,ImageDoor,0.6,posDoor.posX, posDoor.posY,false);
				if(findImageInGameWnd(common::CStringTostring(passThroughImage))){
					break;
				}
				if(iTimes++>=3){
					CKeyMouMng::Ptr()->RoleMoveRandom(600);
				}
				if(posLevel.posX==0){//如果找不到人物的位置，则反方向挪动
					CKeyMouMng::Ptr()->DirKeyDown(VK_RIGHT);
					Sleep(1000);
					CKeyMouMng::Ptr()->DirKeyUp(VK_RIGHT);
					continue;
				}
				if(posDoor.posX == 0){
					CKeyMouMng::Ptr()->DirKeyDown(VK_LEFT);
					Sleep(1000);
					CKeyMouMng::Ptr()->DirKeyUp(VK_LEFT);
					continue;
				}
				if(brick!=0){
					posDoor.posY += 30;
				}else{
					posDoor.posY += 60;
				}
				if(posLevel.posY>posDoor.posY){
					auto distanceY = posLevel.posY-posDoor.posY;
					if( distanceY > 20){
						CKeyMouMng::Ptr()->DirKeyDown(VK_UP);
						Sleep(((float)distanceY)*5.9);
						CKeyMouMng::Ptr()->DirKeyUp(VK_UP);
					}else{
						CKeyMouMng::Ptr()->DirKeyDown(VK_LEFT);
						continue;
					}
				}else if(posLevel.posY<posDoor.posY){
					auto distanceY = posDoor.posY-posLevel.posY;
					if(distanceY > 20){
						CKeyMouMng::Ptr()->DirKeyDown(VK_DOWN);
						Sleep(((float)distanceY)*5.9);
						CKeyMouMng::Ptr()->DirKeyUp(VK_DOWN);
					}else{
						CKeyMouMng::Ptr()->DirKeyDown(VK_LEFT);
						continue;
					}
				}	
				{
					if(brick!=0&&!findImageInGameWnd("Next.png")){
						break;
					}
				}
			}	
			CKeyMouMng::Ptr()->DirKeyUp(VK_LEFT);	
		}
	if(Direction_DOWN == dir){//
		wsprintf(ImageLevel, _T("%sMatchImage\\Game\\Level.png"), g_ExePath);
		wsprintf(ImageDoor, _T("%sMatchImage\\Game\\DownDoor.png"), g_ExePath);
		int iTimes = 0;
		while(true){	
			if(!findImageInGameWnd("Next.png")){
				break;
			}
			imageMatchFromHwnd(hGame,ImageLevel,0.6,posLevel.posX, posLevel.posY,false);
			imageMatchFromHwnd(hGame,ImageDoor,0.6,posDoor.posX, posDoor.posY,false, true, 0, 300);
			if(posDoor.posX == 0){
				CKeyMouMng::Ptr()->DirKeyDown(VK_DOWN);
				Sleep(1000);
				CKeyMouMng::Ptr()->DirKeyUp(VK_DOWN);
				continue;
			}
			if(iTimes++>=5){
				CKeyMouMng::Ptr()->RoleMoveRandom(600);
			}
			posDoor.posX += 30;
			if(posLevel.posX==0){//如果找不到人物的位置，则反方向挪动
				CKeyMouMng::Ptr()->DirKeyDown(VK_RIGHT);
				Sleep(300);
				CKeyMouMng::Ptr()->DirKeyUp(VK_RIGHT);
				continue;
			}
			if(posLevel.posX>posDoor.posX){
				auto distanceX = posLevel.posX-posDoor.posX;
				if(distanceX > 20){
					CKeyMouMng::Ptr()->DirKeyDown(VK_LEFT);
					Sleep(((float)distanceX)*5.8);
					CKeyMouMng::Ptr()->DirKeyUp(VK_LEFT);
				}else{
					CKeyMouMng::Ptr()->DirKeyDown(VK_DOWN);
					Sleep(50);
					CKeyMouMng::Ptr()->DirKeyDown(VK_DOWN);
				}
			}else if(posLevel.posX<posDoor.posX){
				auto distanceX = posDoor.posX-posLevel.posX;
				if(distanceX > 20){
					CKeyMouMng::Ptr()->DirKeyDown(VK_RIGHT);
					Sleep(((float)distanceX)*5.8);
					CKeyMouMng::Ptr()->DirKeyUp(VK_RIGHT);
				}else{
					CKeyMouMng::Ptr()->DirKeyDown(VK_DOWN);
					Sleep(50);
					CKeyMouMng::Ptr()->DirKeyDown(VK_DOWN);
				}
			}	
		}		
		CKeyMouMng::Ptr()->DirKeyUp(VK_DOWN);
	}	
	if(Direction_RIGHT == dir){//
		wsprintf(ImageLevel, _T("%sMatchImage\\Game\\Level.png"), g_ExePath);
		wsprintf(ImageDoor, _T("%sMatchImage\\Game\\RightDoor.png"), g_ExePath);
		if(brick==5){
			wsprintf(ImageDoor, _T("%sMatchImage\\Game\\Brick5Door.png"), g_ExePath);
		}else if(brick==6){
			wsprintf(ImageDoor, _T("%sMatchImage\\Game\\Brick6Door.png"), g_ExePath);
		}else if(brick==7){
			wsprintf(ImageDoor, _T("%sMatchImage\\Game\\Brick7Door.png"), g_ExePath);
		}
		int iTimes = 0;
		while(true){
			imageMatchFromHwnd(hGame,ImageLevel,0.6,posLevel.posX, posLevel.posY,false);
			imageMatchFromHwnd(hGame,ImageDoor,0.6,posDoor.posX, posDoor.posY,false, true, 400, 0);
			if(!findImageInGameWnd("Next.png")){
				break;
			}
			if(iTimes++>=5){
				CKeyMouMng::Ptr()->RoleMoveRandom(600);
			}
			if(posDoor.posX == 0){
				CKeyMouMng::Ptr()->DirKeyDown(VK_RIGHT);
				Sleep(1000);
				CKeyMouMng::Ptr()->DirKeyUp(VK_RIGHT);
				continue;
			}
			if(brick!=5&&brick!=6&&brick!=7){
				posDoor.posY += 30;
			}
			if(posLevel.posY>posDoor.posY){
				auto distanceY = posLevel.posY-posDoor.posY;
				if( distanceY > 20){
					CKeyMouMng::Ptr()->DirKeyDown(VK_UP);
					Sleep(((float)distanceY)*5.8);
					CKeyMouMng::Ptr()->DirKeyUp(VK_UP);
				}else{
					CKeyMouMng::Ptr()->DirKeyDown(VK_RIGHT);
					Sleep(50);
					CKeyMouMng::Ptr()->DirKeyDown(VK_RIGHT);
				}
			}else if(posLevel.posY<posDoor.posY){
				auto distanceY = posDoor.posY-posLevel.posY;
				if(distanceY > 20){
					CKeyMouMng::Ptr()->DirKeyDown(VK_DOWN);
					Sleep(((float)distanceY)*5.8);
					CKeyMouMng::Ptr()->DirKeyUp(VK_DOWN);
				}else{
					CKeyMouMng::Ptr()->DirKeyDown(VK_RIGHT);
					Sleep(50);
					CKeyMouMng::Ptr()->DirKeyDown(VK_RIGHT);
				}
			}
		}
		CKeyMouMng::Ptr()->DirKeyUp(VK_RIGHT);
	}
	if(findImageInGameWnd(common::CStringTostring(passThroughImage), 0.55)){
		return true;
	}
	return false;
}

void CGameControl::FightBrickOne()
{
	//首先释放H技能
	CKeyMouMng::Ptr()->KeyboardButtonEx('h'-32);
	HWND hGame = GetGameWnd();
	if (hGame == NULL) 
		return;
	CKeyMouMng::Ptr()->RoleFastMoveTowards(VK_LEFT);
	Sleep(1500);
	TCHAR ImagePath[MAX_PATH] = {0};
	posInfo posNext;		
	vector<string> skills;
	skills.push_back("q");
	skills.push_back("xxx");
	skills.push_back("w");
	skills.push_back("e");
	skills.push_back("r");
	skills.push_back("t");
	skills.push_back("s");
	skills.push_back("d");
	skills.push_back("f");
	skills.push_back("g");
	skills.push_back("h");

	srand((int)time(0));
	while(true){
		wsprintf(ImagePath, _T("%sMatchImage\\Game\\Next.png"), g_ExePath);
		imageMatchFromHwnd(hGame,ImagePath,0.7,posNext.posX, posNext.posY,false, false);
		if(posNext.posX>=700 &&posNext.posX <=800 && posNext.posY>=27 && posNext.posY<= 124){
			break;
		}
		if(findImageInGameWnd("Brick1Open.png", 0.7)){
			break;
		}
		if(findImageInGameWnd("Brick2.png")){
			break;
		}
		for (auto count(0); count < 3; count++)
		{
			const string skill(skills.at(rand()%skills.size()));
			for (auto i(0); i < skill.size(); i++)
			{
				CKeyMouMng::Ptr()->KeyboardButtonEx(skill.at(i)-32);
				Sleep(50);
			}
		}
		CKeyMouMng::Ptr()->KeyboardButtonEx('s'-32);
		Sleep(50);
	}
	CKeyMouMng::Ptr()->DirKeyUp(VK_LEFT);
	ScanAndGrabObjects(0);
	MoveBrickOne();
}

void CGameControl::MoveBrickOne()
{
	HWND hGame = GetGameWnd();
	if (hGame == NULL) 
		return;
	TCHAR ImageLevel[MAX_PATH] = {0};
	CString passThroughImage;
	passThroughImage.Format(_T("Brick%d.png"), 2);
	posInfo posLevel;
	wsprintf(ImageLevel, _T("%sMatchImage\\Game\\Level.png"), g_ExePath);
	const int brickOneDoorY = 352;
	while(true){
		imageMatchFromHwnd(hGame,ImageLevel,0.6,posLevel.posX, posLevel.posY,false);
		if(findImageInGameWnd(common::CStringTostring(passThroughImage))){
			break;
		}
		if(posLevel.posY==0){//如果找不到人物的位置，则反方向挪动
			CKeyMouMng::Ptr()->RoleFastMoveTowards(VK_RIGHT);
			Sleep(300);
			CKeyMouMng::Ptr()->DirKeyUp(VK_RIGHT);
			continue;
		}
		if(posLevel.posY>brickOneDoorY){
			auto distanceY = brickOneDoorY - posLevel.posY;
			if( distanceY > 5){
				CKeyMouMng::Ptr()->DirKeyDown(VK_UP);
				Sleep(((float)distanceY)*5.9);
				CKeyMouMng::Ptr()->DirKeyUp(VK_UP);
			}else{
				CKeyMouMng::Ptr()->RoleFastMoveTowards(VK_LEFT);
				continue;
			}
		}else if(posLevel.posY<brickOneDoorY){
			auto distanceY = brickOneDoorY-posLevel.posY;
				if(distanceY > 5){
				CKeyMouMng::Ptr()->DirKeyDown(VK_DOWN);
					Sleep(((float)distanceY)*5.9);
				CKeyMouMng::Ptr()->DirKeyUp(VK_DOWN);
			}else{
					CKeyMouMng::Ptr()->RoleFastMoveTowards(VK_LEFT);
				continue;
			}
			}
	}	
	CKeyMouMng::Ptr()->DirKeyUp(VK_LEFT);
}
