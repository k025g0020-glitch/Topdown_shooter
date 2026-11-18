#define NOMINMAX
#include "MiniMap.h"
#include "MapChipField.h"
#include <algorithm>
#include <cmath>

using namespace KamataEngine;

namespace {
// 色
const Vector4 kFloorColor = {0.55f, 0.65f, 0.8f, 0.75f};   // 探索済みの床
const Vector4 kPlayerColor = {1.0f, 0.85f, 0.2f, 1.0f};    // 自キャラ
const Vector4 kBossColor = {1.0f, 0.2f, 0.2f, 1.0f};       // ボス
const Vector4 kMiniBackColor = {0.0f, 0.0f, 0.0f, 0.5f};   // ミニマップの背景
const Vector4 kMapBackColor = {0.0f, 0.0f, 0.0f, 0.55f};   // 全体マップの背景
} // namespace

MiniMap::~MiniMap() {
	delete spriteMiniBack_;
	delete spriteMapBack_;
	for (Sprite* sprite : pool_) {
		delete sprite;
	}
}

void MiniMap::Initialize(const MapChipField* mapChipField) {
	mapChipField_ = mapChipField;
	numCols_ = static_cast<int32_t>(mapChipField->GetNumBlockHorizontal());
	numRows_ = static_cast<int32_t>(mapChipField->GetNumBlockVirtical());
	explored_.assign(static_cast<size_t>(numCols_ * numRows_), false);

	// テクスチャハンドル0（白）を色付けして使う
	float miniLeft = static_cast<float>(WinApp::kWindowWidth) - kMiniMargin - kMiniSize;
	spriteMiniBack_ = Sprite::Create(0, {miniLeft, kMiniMargin});
	spriteMiniBack_->SetSize({kMiniSize, kMiniSize});
	spriteMiniBack_->SetColor(kMiniBackColor);

	spriteMapBack_ = Sprite::Create(0, {0.0f, 0.0f});
	spriteMapBack_->SetSize({static_cast<float>(WinApp::kWindowWidth), static_cast<float>(WinApp::kWindowHeight)});
	spriteMapBack_->SetColor(kMapBackColor);

	pool_.reserve(kPoolSize);
	for (uint32_t i = 0; i < kPoolSize; ++i) {
		pool_.push_back(Sprite::Create(0, {0.0f, 0.0f}));
	}
}

void MiniMap::SetBoss(bool hasBoss, const Vector3& position) {
	hasBoss_ = hasBoss;
	if (hasBoss) {
		MapChipField::IndexSet index = mapChipField_->GetMapChipIndexByPosition(position);
		bossCellX_ = static_cast<float>(index.x);
		bossCellY_ = static_cast<float>(index.y);
	}
}

void MiniMap::Update(const Vector3& playerPosition) {
	// TAB で全体マップの表示を切り替え
	if (Input::GetInstance()->TriggerKey(DIK_TAB)) {
		isMapOpen_ = !isMapOpen_;
	}

	// 自キャラの位置をマス単位に変換（列 = X, 行 = 上ほど奥(+Z)）
	playerCellX_ = playerPosition.x;
	playerCellY_ = static_cast<float>(numRows_ - 1) - playerPosition.z;

	// 別のマスに移ったときだけ周りを探索済みにする
	MapChipField::IndexSet index = mapChipField_->GetMapChipIndexByPosition(playerPosition);
	int32_t cellX = static_cast<int32_t>(index.x);
	int32_t cellY = static_cast<int32_t>(index.y);
	if (cellX != lastCellX_ || cellY != lastCellY_) {
		lastCellX_ = cellX;
		lastCellY_ = cellY;
		Reveal(cellX, cellY);
	}

	if (isDirty_) {
		RebuildRects();
		isDirty_ = false;
	}

	// スプライトを並べ直す
	usedCount_ = 0;
	if (isMapOpen_) {
		// 全体マップ：マップ全体が画面に収まる縮尺で中央に表示
		float areaWidth = static_cast<float>(WinApp::kWindowWidth) - kMapMarginX * 2.0f;
		float areaHeight = static_cast<float>(WinApp::kWindowHeight) - kMapMarginY * 2.0f;
		float cellSize = std::min(areaWidth / static_cast<float>(numCols_), areaHeight / static_cast<float>(numRows_));
		float mapWidth = cellSize * static_cast<float>(numCols_);
		float mapHeight = cellSize * static_cast<float>(numRows_);
		float left = (static_cast<float>(WinApp::kWindowWidth) - mapWidth) / 2.0f;
		float top = (static_cast<float>(WinApp::kWindowHeight) - mapHeight) / 2.0f;
		LayoutMap(left, top, left + mapWidth, top + mapHeight, cellSize, 0.0f, 0.0f);
	} else {
		// ミニマップ：自キャラを中心に周りだけ表示
		float left = static_cast<float>(WinApp::kWindowWidth) - kMiniMargin - kMiniSize;
		float top = kMiniMargin;
		float halfCells = kMiniSize / kMiniCellSize / 2.0f;
		// マスの中心が座標なので 0.5 ずらして、マスの左上を基準にする
		LayoutMap(left, top, left + kMiniSize, top + kMiniSize, kMiniCellSize, playerCellX_ + 0.5f - halfCells, playerCellY_ + 0.5f - halfCells);
	}
}

void MiniMap::Reveal(int32_t centerX, int32_t centerY) {
	for (int32_t dy = -kRevealRadius; dy <= kRevealRadius; ++dy) {
		for (int32_t dx = -kRevealRadius; dx <= kRevealRadius; ++dx) {
			if (dx * dx + dy * dy > kRevealRadius * kRevealRadius) {
				continue;
			}
			int32_t x = centerX + dx;
			int32_t y = centerY + dy;
			if (x < 0 || y < 0 || x >= numCols_ || y >= numRows_) {
				continue;
			}
			size_t i = static_cast<size_t>(y * numCols_ + x);
			if (!explored_[i]) {
				explored_[i] = true;
				isDirty_ = true;
			}
		}
	}
}

void MiniMap::RebuildRects() {
	// 1. 行ごとに「探索済みの床」が横に続く区間を探す
	// 2. 真上の行に同じ区間の長方形があれば、それを下に伸ばしてまとめる
	//    （スプライトの数を減らすため）
	rects_.clear();
	std::vector<int32_t> openRects; // 前の行で伸ばせる長方形の番号

	for (int32_t y = 0; y < numRows_; ++y) {
		std::vector<int32_t> nextOpenRects;
		int32_t x = 0;
		while (x < numCols_) {
			auto isVisibleFloor = [&](int32_t cx) {
				size_t i = static_cast<size_t>(y * numCols_ + cx);
				return explored_[i] && mapChipField_->GetMapChipTypeByIndex(static_cast<uint32_t>(cx), static_cast<uint32_t>(y)) != MapChipType::kBlock;
			};
			if (!isVisibleFloor(x)) {
				++x;
				continue;
			}
			int32_t start = x;
			while (x < numCols_ && isVisibleFloor(x)) {
				++x;
			}
			int32_t width = x - start;

			// 真上の行に同じ区間の長方形があれば伸ばす
			bool merged = false;
			for (int32_t rectIndex : openRects) {
				CellRect& rect = rects_[rectIndex];
				if (rect.x == start && rect.width == width) {
					++rect.height;
					nextOpenRects.push_back(rectIndex);
					merged = true;
					break;
				}
			}
			if (!merged) {
				rects_.push_back({start, y, width, 1});
				nextOpenRects.push_back(static_cast<int32_t>(rects_.size()) - 1);
			}
		}
		openRects = nextOpenRects;
	}
}

void MiniMap::LayoutMap(float clipLeft, float clipTop, float clipRight, float clipBottom, float cellSize, float originX, float originY) {
	// マス座標 → 画面座標
	auto toScreenX = [&](float cellX) { return clipLeft + (cellX - originX) * cellSize; };
	auto toScreenY = [&](float cellY) { return clipTop + (cellY - originY) * cellSize; };

	// 探索済みの床
	for (const CellRect& rect : rects_) {
		float left = toScreenX(static_cast<float>(rect.x));
		float top = toScreenY(static_cast<float>(rect.y));
		float right = toScreenX(static_cast<float>(rect.x + rect.width));
		float bottom = toScreenY(static_cast<float>(rect.y + rect.height));
		// 描いてよい範囲で切り取る
		left = std::max(left, clipLeft);
		top = std::max(top, clipTop);
		right = std::min(right, clipRight);
		bottom = std::min(bottom, clipBottom);
		if (left >= right || top >= bottom) {
			continue;
		}
		if (!PlaceSprite(left, top, right, bottom, kFloorColor)) {
			break;
		}
	}

	// 印（中心の座標と大きさ）を範囲内なら置く
	auto placeMarker = [&](float cellX, float cellY, float size, const Vector4& color) {
		float centerX = toScreenX(cellX + 0.5f);
		float centerY = toScreenY(cellY + 0.5f);
		if (centerX < clipLeft || centerX > clipRight || centerY < clipTop || centerY > clipBottom) {
			return;
		}
		PlaceSprite(centerX - size / 2.0f, centerY - size / 2.0f, centerX + size / 2.0f, centerY + size / 2.0f, color);
	};

	// ボス（探索済みの場所にいるときだけ）
	if (hasBoss_) {
		int32_t bx = static_cast<int32_t>(bossCellX_);
		int32_t by = static_cast<int32_t>(bossCellY_);
		if (bx >= 0 && by >= 0 && bx < numCols_ && by < numRows_ && explored_[static_cast<size_t>(by * numCols_ + bx)]) {
			placeMarker(bossCellX_, bossCellY_, std::max(cellSize * 3.0f, 10.0f), kBossColor);
		}
	}

	// 自キャラ
	placeMarker(playerCellX_, playerCellY_, std::max(cellSize * 1.5f, 6.0f), kPlayerColor);
}

bool MiniMap::PlaceSprite(float left, float top, float right, float bottom, const Vector4& color) {
	if (usedCount_ >= pool_.size()) {
		return false;
	}
	Sprite* sprite = pool_[usedCount_++];
	sprite->SetPosition({left, top});
	sprite->SetSize({right - left, bottom - top});
	sprite->SetColor(color);
	return true;
}

void MiniMap::Draw() {
	Sprite::PreDraw();
	if (isMapOpen_) {
		spriteMapBack_->Draw();
	} else {
		spriteMiniBack_->Draw();
	}
	for (uint32_t i = 0; i < usedCount_; ++i) {
		pool_[i]->Draw();
	}
	Sprite::PostDraw();
}
