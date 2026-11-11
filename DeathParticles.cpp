#include "DeathParticles.h"
#include "calc.h"

using namespace KamataEngine;

void DeathParticles::Initialize(Model* model, Camera* camera, const Vector3& position) {
	// メンバ変数への代入
	model_ = model;
	camera_ = camera;

	// ワールド変換の初期化
	for (WorldTransform& worldTransform : worldTransform_) {
		worldTransform.Initialize();
		worldTransform.translation_ = position;
	}

	// 色変更オブジェクトとカラー数値の初期化
	objectColor_.Initialize();
	color_ = {1.0f, 1.0f, 1.0f, 1.0f};

	// 初期化時にタイマーとフラグをリセット
	counter_ = 0.0f;
	isFinished_ = false;
}

void DeathParticles::Update() {

	if (isFinished_) {
		return;
	}

	// 8個分の速度ベクトルを計算して座標を進める
	for (uint32_t i = 0; i < kNumParticles; ++i) {
		// 基本となる速度ベクトル
		Vector3 velocity = {kSpeed, 0.0f, 0.0f};

		// 回転角を計算する
		float angle = kAngleUnit * static_cast<float>(i);

		// Z軸まわり回転行列
		Matrix4x4 matrixRotation = calc::MakeRotateZMatrix(angle);

		// 基本ベクトルを回転させて速度ベクトルを得る
		velocity = calc::Transform(velocity, matrixRotation);

		// 移動処理
		worldTransform_[i].translation_.x += velocity.x;
		worldTransform_[i].translation_.y += velocity.y;
		worldTransform_[i].translation_.z += velocity.z;
	}

	// ワールド変換の更新
	for (WorldTransform& worldTransform : worldTransform_) {
		// アフィン変換行列の計算
		worldTransform.matWorld_ = calc::MakeAffineMatrix(worldTransform.scale_, worldTransform.rotation_, worldTransform.translation_);
		// VRAMに転送
		worldTransform.TransferMatrix();
	}

	// 時間カウントの更新
	counter_ += 1.0f / 60.0f;
	if (counter_ >= kDuration) {
		counter_ = kDuration;
		isFinished_ = true;
	}

	// カウンターの値に応じてアルファ値を1.0→0.0へフェードアウト
	color_.w = std::clamp(1.0f - (counter_ / kDuration), 0.0f, 1.0f);
	// 色変更オブジェクトに色の数値を設定する
	objectColor_.SetColor(color_);
}

void DeathParticles::Draw() {

	if (isFinished_) {
		return;
	}

	// モデルの描画 (第三引数に色変更オブジェクトを指定)
	for (WorldTransform& worldTransform : worldTransform_) {
		model_->Draw(worldTransform, *camera_, &objectColor_);
	}
}