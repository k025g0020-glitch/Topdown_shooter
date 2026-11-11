#include "WorldTransformConfig.h"
#include "calc.h"

void WorldTransformConfig::UpdateWorldTransform(KamataEngine::WorldTransform& worldTransform) {
	// スケール、回転、平行移動を合成して行列を計算する
	worldTransform.matWorld_ = calc::MakeAffineMatrix(worldTransform.scale_, worldTransform.rotation_, worldTransform.translation_);

	// 定数バッファへの書き込み
	worldTransform.TransferMatrix();
}