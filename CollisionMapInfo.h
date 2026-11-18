#pragma once
#include "KamataEngine.h"

// 見下ろし型（XZ平面）用のマップ衝突情報
struct CollisionMapInfo {
	bool hitWallX = false;           // X方向（左右）の壁に接触
	bool hitWallZ = false;           // Z方向（奥・手前）の壁に接触
	KamataEngine::Vector3 move = {}; // 移動量
};

struct AABB {
	KamataEngine::Vector3 min; // 最小座標
	KamataEngine::Vector3 max; // 最大座標
};
