#include "calc.h"
#include <algorithm> // std::swap用
#include <cmath>

// 加算
Vector3 calc::Add(const Vector3& v1, const Vector3& v2) { return {v1.x + v2.x, v1.y + v2.y, v1.z + v2.z}; }

// 減算
Vector3 calc::Subtract(const Vector3& v1, const Vector3& v2) { return {v1.x - v2.x, v1.y - v2.y, v1.z - v2.z}; }

// スカラー倍
Vector3 calc::Multiply(float scalar, const Vector3& v) { return {scalar * v.x, scalar * v.y, scalar * v.z}; }

// 内積
float calc::Dot(const Vector3& v1, const Vector3& v2) { return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z; }

// 長さ（ノルム）
float calc::Length(const Vector3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

// 正規化
Vector3 calc::Normalize(const Vector3& v) {
	float len = Length(v);
	if (len != 0.0f) {
		return {v.x / len, v.y / len, v.z / len};
	}
	return {0.0f, 0.0f, 0.0f};
}

// 数値 (float) の線形補間
float calc::Lerp(float v1, float v2, float t) { return v1 + (v2 - v1) * t; }

// ベクトル (Vector3) の線形補間
Vector3 calc::Lerp(const Vector3& v1, const Vector3& v2, float t) {
	Vector3 diff = calc::Subtract(v2, v1);
	Vector3 scaledDiff = calc::Multiply(t, diff);
	return calc::Add(v1, scaledDiff);
}

// 行列の加法
Matrix4x4 calc::Add(const Matrix4x4& m1, const Matrix4x4& m2) {
	Matrix4x4 result;
	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++j)
			result.m[i][j] = m1.m[i][j] + m2.m[i][j];
	return result;
}

// 行列の減法
Matrix4x4 calc::Subtract(const Matrix4x4& m1, const Matrix4x4& m2) {
	Matrix4x4 result;
	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++j)
			result.m[i][j] = m1.m[i][j] - m2.m[i][j];
	return result;
}

// 行列の積
Matrix4x4 calc::Multiply(const Matrix4x4& m1, const Matrix4x4& m2) {
	Matrix4x4 result = {};
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			for (int k = 0; k < 4; ++k) {
				result.m[i][j] += m1.m[i][k] * m2.m[k][j];
			}
		}
	}
	return result;
}

// 転置行列
Matrix4x4 calc::Transpose(const Matrix4x4& m) {
	Matrix4x4 result;
	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++j)
			result.m[i][j] = m.m[j][i];
	return result;
}

// 単位行列
Matrix4x4 calc::MakeIdentity4x4() {
	Matrix4x4 res = {};
	res.m[0][0] = 1.0f;
	res.m[1][1] = 1.0f;
	res.m[2][2] = 1.0f;
	res.m[3][3] = 1.0f;
	return res;
}

// 逆行列
Matrix4x4 calc::Inverse(const Matrix4x4& m) {
	float sweep[4][8];
	float delta = 1.0e-5f;
	Matrix4x4 res;

	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			sweep[i][j] = m.m[i][j];
			sweep[i][j + 4] = (i == j) ? 1.0f : 0.0f;
		}
	}

	for (int k = 0; k < 4; k++) {
		float max = std::abs(sweep[k][k]);
		int pivot = k;
		for (int i = k + 1; i < 4; i++) {
			if (std::abs(sweep[i][k]) > max) {
				max = std::abs(sweep[i][k]);
				pivot = i;
			}
		}

		if (max < delta)
			return MakeIdentity4x4();

		if (k != pivot) {
			for (int j = 0; j < 8; j++)
				std::swap(sweep[k][j], sweep[pivot][j]);
		}

		float a = sweep[k][k];
		for (int j = 0; j < 8; j++)
			sweep[k][j] /= a;

		for (int i = 0; i < 4; i++) {
			if (i != k) {
				float b = sweep[i][k];
				for (int j = 0; j < 8; j++)
					sweep[i][j] -= b * sweep[k][j];
			}
		}
	}

	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			res.m[i][j] = sweep[i][j + 4];
		}
	}
	return res;
}

// 平行移動行列
Matrix4x4 calc::MakeTranslateMatrix(const Vector3& translate) {
	Matrix4x4 res = MakeIdentity4x4();
	res.m[3][0] = translate.x;
	res.m[3][1] = translate.y;
	res.m[3][2] = translate.z;
	return res;
}

// 拡大縮小行列
Matrix4x4 calc::MakeScaleMatrix(const Vector3& scale) {
	Matrix4x4 res = {};
	res.m[0][0] = scale.x;
	res.m[1][1] = scale.y;
	res.m[2][2] = scale.z;
	res.m[3][3] = 1.0f;
	return res;
}

// 座標変換
Vector3 calc::Transform(const Vector3& vector, const Matrix4x4& matrix) {
	Vector3 result;
	result.x = vector.x * matrix.m[0][0] + vector.y * matrix.m[1][0] + vector.z * matrix.m[2][0] + 1.0f * matrix.m[3][0];
	result.y = vector.x * matrix.m[0][1] + vector.y * matrix.m[1][1] + vector.z * matrix.m[2][1] + 1.0f * matrix.m[3][1];
	result.z = vector.x * matrix.m[0][2] + vector.y * matrix.m[1][2] + vector.z * matrix.m[2][2] + 1.0f * matrix.m[3][2];
	float w = vector.x * matrix.m[0][3] + vector.y * matrix.m[1][3] + vector.z * matrix.m[2][3] + 1.0f * matrix.m[3][3];

	if (w != 0.0f) {
		result.x /= w;
		result.y /= w;
		result.z /= w;
	}
	return result;
}

// X軸回転行列
Matrix4x4 calc::MakeRotateXMatrix(float radian) {
	float s = std::sin(radian);
	float c = std::cos(radian);
	Matrix4x4 res = MakeIdentity4x4();
	res.m[1][1] = c;
	res.m[1][2] = s;
	res.m[2][1] = -s;
	res.m[2][2] = c;
	return res;
}

// Y軸回転行列
Matrix4x4 calc::MakeRotateYMatrix(float radian) {
	float s = std::sin(radian);
	float c = std::cos(radian);
	Matrix4x4 res = MakeIdentity4x4();
	res.m[0][0] = c;
	res.m[0][2] = -s;
	res.m[2][0] = s;
	res.m[2][2] = c;
	return res;
}

// Z軸回転行列
Matrix4x4 calc::MakeRotateZMatrix(float radian) {
	float s = std::sin(radian);
	float c = std::cos(radian);
	Matrix4x4 res = MakeIdentity4x4();
	res.m[0][0] = c;
	res.m[0][1] = s;
	res.m[1][0] = -s;
	res.m[1][1] = c;
	return res;
}

// 3次元アフィン変換行列
Matrix4x4 calc::MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate) {
	Matrix4x4 scaleMatrix = MakeScaleMatrix(scale);

	Matrix4x4 rotateXMatrix = MakeRotateXMatrix(rotate.x);
	Matrix4x4 rotateYMatrix = MakeRotateYMatrix(rotate.y);
	Matrix4x4 rotateZMatrix = MakeRotateZMatrix(rotate.z);
	Matrix4x4 rotateXYZMatrix = Multiply(rotateXMatrix, Multiply(rotateYMatrix, rotateZMatrix));

	Matrix4x4 translateMatrix = MakeTranslateMatrix(translate);

	// S * R * T
	return Multiply(scaleMatrix, Multiply(rotateXYZMatrix, translateMatrix));
}

// AABB同士の交差判定関数
bool calc::IsCollision(const AABB& aabb1, const AABB& aabb2) {
	if ((aabb1.min.x <= aabb2.max.x && aabb1.max.x >= aabb2.min.x) && (aabb1.min.y <= aabb2.max.y && aabb1.max.y >= aabb2.min.y) && (aabb1.min.z <= aabb2.max.z && aabb1.max.z >= aabb2.min.z)) {
		return true;
	}
	return false;
}