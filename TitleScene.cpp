#include "TitleScene.h"

using namespace KamataEngine;

TitleScene::~TitleScene() {
	// フェードの解放
	delete fade_;
}

void TitleScene::Initialize() {
	finished_ = false;

	// フェードの生成と初期化
	fade_ = new Fade();
	fade_->Initialize();

	// 初期フェーズ設定とフェードイン開始
	phase_ = Phase::kFadeIn;
	fade_->Start(Fade::Status::FadeIn, kFadeDuration);
}

void TitleScene::Update() {
	// フェードの更新
	if (fade_) {
		fade_->Update();
	}

	switch (phase_) {
	case Phase::kFadeIn:
		// フェードインが終了したらメインフェーズへ移行
		if (fade_->IsFinished()) {
			fade_->Stop();
			phase_ = Phase::kMain;
		}
		break;

	case Phase::kMain:
		// スペースキーでフェードアウトフェーズへ移行
		if (Input::GetInstance()->PushKey(DIK_SPACE)) {
			phase_ = Phase::kFadeOut;
			fade_->Start(Fade::Status::FadeOut, kFadeDuration);
		}
		break;

	case Phase::kFadeOut:
		// フェードアウトが終了したらシーン終了フラグを立てる
		if (fade_->IsFinished()) {
			finished_ = true;
		}
		break;
	}
}

void TitleScene::Draw() {
	// フェードの描画
	if (fade_) {
		fade_->Draw();
	}
}