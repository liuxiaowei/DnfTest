#include "StdAfx.h"
#include "MapPosition.h"


CMapPosition::CMapPosition(void)
{
}


CMapPosition::~CMapPosition(void)
{
}

Direction CMapPosition::getNextDirection(const posInfo&Left, const posInfo& right)
{
	if(right.posX - Left.posX >= 16){
		return Direction_RIGHT;
	}
	if(right.posX - Left.posX <= -16){
		return Direction_LEFT;
	}
	if(right.posY - Left.posY >= 16){
		return Direction_DOWN;
	}
	if(right.posY - Left.posY <= -16){
		return Direction_UP;
	}
	return Direction_NULL;
}
