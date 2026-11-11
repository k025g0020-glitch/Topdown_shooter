#pragma once
#include "KamataEngine.h"

struct CollisionMapInfo {
	bool ceilingCollision = false; // 天井衝突フラグ
	bool landing = false;          // 着地フラグ
	bool wallCollision = false;    // 壁接触フラグ
	bool hitWall = false;
	KamataEngine::Vector3 move = {}; // 移動量
};

struct AABB {
	KamataEngine::Vector3 min; // 最小座標
	KamataEngine::Vector3 max; // 最大座標
};