﻿#pragma once
#include "baseentity.h"

struct FileWeaponInfo_t
{
private:
	byte pad_0x0000[0x4];		// 0x0000 - VTable

public:
	bool bParsedScript;
	bool bLoadedHudElements;
	char szClassName[80];
	char szPrintName[80];
	char szViewModel[80];
	char szWorldModel[80];
	char szAnimationPrefix[16];
	std::int16_t pad1;
	std::int32_t iSlot;
	std::int32_t iPosition;
	std::int32_t iMaxClip1;
	std::int32_t iMaxClip2;
	std::int32_t iDefaultClip1;
	std::int32_t iDefaultClip12;
	std::int32_t iWeight;
	std::int32_t iRumble;
	bool bAutoSwitchTo;
	bool bAutoSwitchFrom;
	std::int32_t iFlags;
};

class CBasePlayer;

class CBaseWeapon : public CBaseEntity
{
public:
	float& GetSpread();
	FileWeaponInfo_t* GetWeaponInfo();
	FileWeaponInfo_t* GetWeaponData();
	const char* GetWeaponName();
	int GetWeaponID();
	int GetWeaponId();
	void UpdateSpread();

	// 武器持有人
	CBasePlayer* GetOwner();

	// 弹夹子弹数量
	int GetClip();

	// 是否正在填装
	bool IsReloading();

	// 弹药类型
	int GetAmmoType();

	// 备用弹药数量
	int GetAmmo();

	// 下一次可以开枪的时间
	float GetNextPrimaryAttack();

	// 距离下一次可以开枪的间隔
	float GetPrimaryAttackDelay();

	// 下一次可以推的时间
	float GetNextSecondryAttack();

	// 距离下一次可以推的间隔
	float GetSecondryAttackDelay();

	// 现在是否可以开枪
	bool CanFire();

	// 是否为枪械
	bool IsFireGun();

	// 现在是否可以推(或者抓)
	bool CanShove();
	
	// 获取近战武器前摇时间(伤害造成的延迟)
	float GetMeleeDelay();
};
