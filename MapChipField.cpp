#include "MapChipField.h"
#include <cassert>
#include <fstream>
#include <sstream>

using namespace KamataEngine;

void MapChipField::Initialize() {}

void MapChipField::Update() {}

void MapChipField::Draw() {}

void MapChipField::LoadMapChipCsv(const std::string& filePath) {
	std::ifstream file;
	file.open(filePath);
	assert(file.is_open());

	// 1行ずつ読み、カンマ区切りの数字を取り出す
	std::vector<std::vector<int>> numbers;
	std::string line;
	while (std::getline(file, line)) {
		// Windows の改行(\r)が残っていたら取り除く
		if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}
		if (line.empty()) {
			continue;
		}
		std::vector<int> row;
		std::istringstream lineStream(line);
		std::string word;
		while (std::getline(lineStream, word, ',')) {
			row.push_back(word.empty() ? 0 : std::stoi(word));
		}
		numbers.push_back(row);
	}
	file.close();
	assert(!numbers.empty());

	// 行数・列数はCSVの大きさで決める
	numBlockVertical_ = static_cast<uint32_t>(numbers.size());
	numBlockHorizontal_ = static_cast<uint32_t>(numbers[0].size());

	mapChipData_.data.assign(numBlockVertical_, std::vector<MapChipType>(numBlockHorizontal_, MapChipType::kBlock));
	bossRoom_.assign(numBlockVertical_, std::vector<bool>(numBlockHorizontal_, false));
	spawnPoints_.clear();

	for (uint32_t y = 0; y < numBlockVertical_; ++y) {
		for (uint32_t x = 0; x < numBlockHorizontal_ && x < numbers[y].size(); ++x) {
			int number = numbers[y][x];
			IndexSet index = {x, y};
			MapChipType type = MapChipType::kBlank;

			switch (number) {
			case 1:
				type = MapChipType::kBlock;
				break;
			case 2:
				type = MapChipType::kDoor;
				break;
			// 3～7 は配置マーカー（マス自体は床）
			case 3:
				spawnPoints_.push_back({SpawnType::kChaserGroup, index});
				break;
			case 4:
				spawnPoints_.push_back({SpawnType::kShooterGroup, index});
				break;
			case 5:
				spawnPoints_.push_back({SpawnType::kSwarmGroup, index});
				break;
			case 6:
				spawnPoints_.push_back({SpawnType::kBoss, index});
				bossRoom_[y][x] = true;
				break;
			case 7:
				spawnPoints_.push_back({SpawnType::kPlayerStart, index});
				break;
			case 8:
				bossRoom_[y][x] = true;
				break;
			default:
				break;
			}
			mapChipData_.data[y][x] = type;
		}
	}
}

MapChipType MapChipField::GetMapChipTypeByIndex(uint32_t xIndex, uint32_t yIndex) const {
	// マップの外は壁あつかい（外に出られないように）
	if (xIndex >= numBlockHorizontal_ || yIndex >= numBlockVertical_) {
		return MapChipType::kBlock;
	}
	return mapChipData_.data[yIndex][xIndex];
}

void MapChipField::SetMapChipTypeByIndex(uint32_t xIndex, uint32_t yIndex, MapChipType type) {
	if (xIndex >= numBlockHorizontal_ || yIndex >= numBlockVertical_) {
		return;
	}
	mapChipData_.data[yIndex][xIndex] = type;
}

bool MapChipField::IsBossRoomByIndex(uint32_t xIndex, uint32_t yIndex) const {
	if (xIndex >= numBlockHorizontal_ || yIndex >= numBlockVertical_) {
		return false;
	}
	return bossRoom_[yIndex][xIndex];
}

// XZ平面上に配置する（CSVの上の行ほど奥(+Z)になる）。高さ(Y)は0
Vector3 MapChipField::GetMapChipPositionByIndex(uint32_t xIndex, uint32_t yIndex) const {
	return Vector3(kBlockWidth * static_cast<float>(xIndex), 0.0f, kBlockHeight * (static_cast<float>(numBlockVertical_) - 1.0f - static_cast<float>(yIndex)));
}

MapChipField::IndexSet MapChipField::GetMapChipIndexByPosition(const Vector3& position) const {
	IndexSet index = {};
	index.x = static_cast<uint32_t>((position.x + kBlockWidth / 2.0f) / kBlockWidth);
	// 奥行き(Z)からCSVの行番号を求める
	uint32_t yIndexBeforeInverse = static_cast<uint32_t>((position.z + kBlockHeight / 2.0f) / kBlockHeight);
	index.y = numBlockVertical_ - 1 - yIndexBeforeInverse;
	return index;
}

MapChipField::Rect MapChipField::GetRectByIndex(uint32_t xIndex, uint32_t yIndex) const {
	// 指定ブロックの中心座標を取得する
	Vector3 center = GetMapChipPositionByIndex(xIndex, yIndex);

	Rect rect;
	rect.left = center.x - kBlockWidth / 2.0f;
	rect.right = center.x + kBlockWidth / 2.0f;
	rect.bottom = center.z - kBlockHeight / 2.0f;
	rect.top = center.z + kBlockHeight / 2.0f;

	return rect;
}
