#pragma once

class posInfo{
public:
	int posX;//������
	int posY;//������
};

class CMapPosition
{
public:
	CMapPosition(void);
	~CMapPosition(void);
	Direction getNextDirection(const posInfo&Left, const posInfo& right);
};

