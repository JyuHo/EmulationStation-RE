#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace Utils
{
	// SHA-256 (FIPS 180-4), to check what ES downloads against a published hash.
	class Sha256
	{
	public:
		Sha256();

		void update(const void* data, size_t length);
		std::string hexDigest(); // finishes the hash: call update() no more after it

		static std::string ofString(const std::string& data);
		static std::string ofFile(const std::string& path); // empty if the file cannot be read

	private:
		void transform(const uint8_t* block);

		uint32_t mState[8];
		uint8_t  mBuffer[64];
		uint64_t mLength;
		size_t   mBufferSize;
	};
}
