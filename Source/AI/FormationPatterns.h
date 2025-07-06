#include <vector>

#ifndef FORMATION_PATTERNS_H_
#define FORMATION_PATTERNS_H_

namespace AI
{
	enum class FormationType
	{
		TriangleSmall,
		TriangleLarge,
		TwoRows8,
		TwoRows12,
		Circle8,
		Circle12,
		VSmall,
		VLarge
	};

	inline std::vector<GW::MATH::GVECTORF> Triangle(uint32_t rows, float spacing)
	{
		std::vector<GW::MATH::GVECTORF> out;
		for (uint32_t r = 0; r < rows; ++r)
			for (uint32_t c = 0; c <= r; ++c)
				out.push_back({ (c - r * 0.5f) * spacing, 0, -static_cast<float>(r) * spacing, 1 });
		return out;
	}

	inline std::vector<GW::MATH::GVECTORF> TwoRows(uint32_t countEven, float spacing)
	{
		std::vector<GW::MATH::GVECTORF> out;
		uint32_t half = countEven / 2;
		for (uint32_t i = 0; i < half; ++i)
		{
			float x = (i - half * 0.5f + 0.5f) * spacing;
			out.push_back({ x, 0, 0, 1 });
			out.push_back({ x, 0, -spacing, 1 });
		}
		return out;
	}

	inline std::vector<GW::MATH::GVECTORF> Circle(uint32_t count, float radius)
	{
		std::vector<GW::MATH::GVECTORF> out;
		out.reserve(count);
		const float step = 6.2831853f / count;
		for (uint32_t i = 0; i < count; ++i)
		{
			float a = step * i;
			float x = radius * std::cos(a);
			float z = radius * -std::sin(a);
			out.push_back({ x, 0, z, 1 });
		}
		return out;
	}

	inline std::vector<GW::MATH::GVECTORF> VShape(uint32_t rows, float spacing)
	{
		std::vector<GW::MATH::GVECTORF> out;
		out.reserve(rows * 2);
		for (uint32_t i = 1; i <= rows; ++i)
		{
			float off = i * spacing;
			out.push_back({ -off, 0,  off, 1 });
			out.push_back({ off, 0,  off, 1 });
		}
		return out;
	}

	inline std::vector<GW::MATH::GVECTORF> MakeSlots(FormationType kind, float spacing = 2.5f)
	{
		switch (kind)
		{
		case FormationType::TriangleSmall: return Triangle(3, spacing);
		case FormationType::TriangleLarge: return Triangle(5, spacing);
		case FormationType::TwoRows8: return TwoRows(8, spacing);
		case FormationType::TwoRows12: return TwoRows(12, spacing);
		case FormationType::Circle8: return Circle(8, 3 * spacing);
		case FormationType::Circle12: return Circle(12, 3 * spacing);
		case FormationType::VSmall: return VShape(3, spacing);
		case FormationType::VLarge: return VShape(6, spacing);
		default:
			return {};
		}
	}
}
#endif