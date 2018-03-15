﻿#pragma once
#include "../Interfaces/IBaseClientState.h"
#include "../interfaces.h"
#include "../../l4d2Simple2/drawing.h"
#include <imgui.h>

#define GetLocalPlayer()		(dynamic_cast<CBaseEntity*>(g_pClientInterface->EntList->GetClientEntity(g_pClientInterface->Engine->GetLocalPlayer())))
#define GetServerTime()			(g_pClientInterface->GlobalVars->interval_per_tick * GetLocalPlayer()->GetNetProp<int>(XorStr("DT_BasePlayer"), XorStr("m_nTickBase")))
#define IsPlayerAlive(_p)		(_p->GetNetProp<LifeStates_t>(XorStr("DT_BasePlayer"), XorStr("m_lifeState")) == LIFE_ALIVE)
#define IsPlayerInLadder(_p)	(_p->GetNetProp<MoveType>(XorStr("DT_BasePlayer"), XorStr("movetype")) == MOVETYPE_LADDER)
#define IsPlayerInGround(_p)	(_p->GetNetProp<MoveType>(XorStr("DT_BasePlayer"), XorStr("m_fFlags")) & FL_ONGROUND)

// 接口类
class CBaseFeatures
{
public:
	CBaseFeatures();
	virtual ~CBaseFeatures();

	virtual void OnConnect();
	virtual void OnDisconnect();

	virtual void OnCreateMove(CUserCmd*, bool*);
	virtual void OnPaintTraverse(VPANEL);
	virtual void OnEnginePaint(PaintMode_t);
	virtual bool OnUserMessage(int, bf_read);
	virtual void OnFrameStageNotify(ClientFrameStage_t);
	virtual bool OnProcessGetCvarValue(SVC_GetCvarValue*, std::string&);
	virtual bool OnProcessSetConVar(NET_SetConVar*);
	virtual bool OnProcessClientCommand(NET_StringCmd*);
	virtual void OnSceneEnd();
	virtual void OnMenuDrawing();
	virtual void OnScreenDrawing();
	virtual bool OnSendMove();
	virtual bool OnFindMaterial(std::string&, std::string&);
	virtual void OnKeyInput(bool, ButtonCode_t, const char*);

private:
	size_t m_iHookIndex;
};
