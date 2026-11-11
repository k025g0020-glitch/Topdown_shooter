#include "MapChipField.h"
#include <fstream>
#include <map>
#include <sstream>
#include <algorithm>
#include <cassert>

using namespace KamataEngine;

namespace {

std::map<std::string, MapChipType> mapChipTable = {
    {"0", MapChipType::kBlank},
    {"1", MapChipType::kBlock},
};
}

void MapChipField::Initialize() {}

void MapChipField::Update() {}

void MapChipField::Draw() {}

void MapChipField::ResetMapChipData() {
	mapChipData_.data.clear();
	mapChipData_.data.resize(kNumBlockVirtical);
	for (std::vector<MapChipType>& mapChipDataLine : mapChipData_.data) {
		mapChipDataLine.resize(kNumBlockHorizontal);
	}
}

void MapChipField::LoadMapChipCsv(const std::string& filePath) {
	ResetMapChipData();

	std::ifstream file;
	file.open(filePath);
	assert(file.is_open());

	std::stringstream mapChipCsv;
	mapChipCsv << file.rdbuf();
	file.close();

	for (uint32_t i = 0; i < kNumBlockVirtical; ++i) {
		std::string line;
		getline(mapChipCsv, line);

		std::istringstream lineStream(line);

		for (uint32_t j = 0; j < kNumBlockHorizontal; ++j) {

			std::string word;
			std::getline(lineStream, word, ',');

			if (mapChipTable.contains(word)) {
				mapChipData_.data[i][j] = mapChipTable[word];
			}
		}
	}
}

MapChipType MapChipField::GetMapChipTypeByIndex(uint32_t xIndex, uint32_t yIndex) {
	if (xIndex >= kNumBlockHorizontal || yIndex >= kNumBlockVirtical) {
		return MapChipType::kBlank;
	}
	return mapChipData_.data[yIndex][xIndex];
}

// XZ平面上に配置する（CSVの上の行ほど奥(+Z)になる）。高さ(Y)は0
Vector3 MapChipField::GetMapChipPositionByIndex(uint32_t xIndex, uint32_t yIndex) {
	return Vector3(kBlockWidth * static_cast<float>(xIndex), 0.0f, kBlockHeight * (static_cast<float>(kNumBlockVirtical) - 1.0f - static_cast<float>(yIndex)));
}

MapChipField::IndexSet MapChipField::GetMapChipIndexByPosition(const Vector3& position) {
	IndexSet index = {};
	index.x = static_cast<uint32_t>((position.x + kBlockWidth / 2.0f) / kBlockWidth);
	// 奥行き(Z)からCSVの行番号を求める
	uint32_t yIndexBeforeInverse = static_cast<uint32_t>((position.z + kBlockHeight / 2.0f) / kBlockHeight);
	index.y = kNumBlockVirtical - 1 - yIndexBeforeInverse;
	return index;
}

MapChipField::Rect MapChipField::GetRectByIndex(uint32_t xIndex, uint32_t yIndex) {
	// 指定ブロックの中心座標を取得する
	Vector3 center = GetMapChipPositionByIndex(xIndex, yIndex);

	Rect rect;
	rect.left = center.x - kBlockWidth / 2.0f;
	rect.right = center.x + kBlockWidth / 2.0f;
	rect.bottom = center.z - kBlockHeight / 2.0f;
	rect.top = center.z + kBlockHeight / 2.0f;

	return rect;
}