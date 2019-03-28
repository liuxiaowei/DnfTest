#pragma once

class posInfo{
public:
	int posX;//ºá×ø±ê
	int posY;//×Ý×ø±ê
};

class CMapPosition
{
public:
	CMapPosition(void);
	~CMapPosition(void);
	Direction getNextDirection(const posInfo&Left, const posInfo& right);
};

