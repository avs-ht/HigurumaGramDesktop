// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Privacy strip: at-rest field encryption for the local message archive.
#include "ayu/utils/ayu_crypto.h"

#include "base/random.h"

#include <QtCore/QByteArray>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <array>
#include <cstring>
#include <memory>

extern "C" {
#include <openssl/evp.h>
} // extern "C"

namespace AyuCrypto {
namespace {

constexpr auto kKeySize = 32; // AES-256
constexpr auto kIvSize = 12; // GCM standard nonce size
constexpr auto kTagSize = 16; // GCM auth tag

using Key = std::array<unsigned char, kKeySize>;

QString KeyFilePath() {
	return QString::fromUtf8("./tdata/ayudata.key");
}

const Key &LoadOrCreateKey() {
	static const auto key = [] {
		auto result = Key();

		auto file = QFile(KeyFilePath());
		if (file.exists() && file.open(QIODevice::ReadOnly)) {
			const auto data = file.readAll();
			if (data.size() == kKeySize) {
				memcpy(result.data(), data.constData(), kKeySize);
				return result;
			}
		}

		base::RandomFill(result.data(), result.size());

		QDir().mkpath(QFileInfo(KeyFilePath()).absolutePath());
		auto out = QFile(KeyFilePath());
		if (out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			out.write(
				reinterpret_cast<const char*>(result.data()),
				qint64(result.size()));
			out.close();
#ifndef Q_OS_WIN
			out.setPermissions(
				QFileDevice::ReadOwner | QFileDevice::WriteOwner);
#endif // !Q_OS_WIN
		}

		return result;
	}();
	return key;
}

struct CipherContextDeleter {
	void operator()(EVP_CIPHER_CTX *ctx) const {
		if (ctx) {
			EVP_CIPHER_CTX_free(ctx);
		}
	}
};
using CipherContext = std::unique_ptr<EVP_CIPHER_CTX, CipherContextDeleter>;

} // namespace

std::vector<char> EncryptBlob(const std::vector<char> &plaintext) {
	if (plaintext.empty()) {
		return {};
	}

	const auto &key = LoadOrCreateKey();

	auto iv = std::array<unsigned char, kIvSize>();
	base::RandomFill(iv.data(), iv.size());

	auto ciphertext = std::vector<unsigned char>(plaintext.size());
	auto tag = std::array<unsigned char, kTagSize>();

	auto ctx = CipherContext(EVP_CIPHER_CTX_new());
	if (!ctx) {
		return plaintext;
	}

	auto len = 0;
	auto ciphertextLen = 0;
	auto ok = EVP_EncryptInit_ex(
			ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr)
		&& EVP_CIPHER_CTX_ctrl(
			ctx.get(), EVP_CTRL_GCM_SET_IVLEN, kIvSize, nullptr)
		&& EVP_EncryptInit_ex(
			ctx.get(), nullptr, nullptr, key.data(), iv.data());
	if (ok) {
		ok = EVP_EncryptUpdate(
			ctx.get(),
			ciphertext.data(),
			&len,
			reinterpret_cast<const unsigned char*>(plaintext.data()),
			int(plaintext.size()));
		ciphertextLen = len;
	}
	if (ok) {
		ok = EVP_EncryptFinal_ex(
			ctx.get(), ciphertext.data() + ciphertextLen, &len);
		ciphertextLen += len;
	}
	if (ok) {
		ok = EVP_CIPHER_CTX_ctrl(
			ctx.get(), EVP_CTRL_GCM_GET_TAG, kTagSize, tag.data());
	}
	if (!ok) {
		// Encryption failed for some reason (should not normally happen).
		// Fail open rather than silently dropping the data.
		return plaintext;
	}

	auto result = std::vector<char>();
	result.reserve(kIvSize + kTagSize + ciphertextLen);
	result.insert(
		result.end(),
		reinterpret_cast<const char*>(iv.data()),
		reinterpret_cast<const char*>(iv.data()) + iv.size());
	result.insert(
		result.end(),
		reinterpret_cast<const char*>(tag.data()),
		reinterpret_cast<const char*>(tag.data()) + tag.size());
	result.insert(
		result.end(),
		reinterpret_cast<const char*>(ciphertext.data()),
		reinterpret_cast<const char*>(ciphertext.data()) + ciphertextLen);
	return result;
}

std::vector<char> DecryptBlob(const std::vector<char> &stored) {
	if (stored.empty()) {
		return {};
	}
	if (stored.size() < kIvSize + kTagSize) {
		// Too short to be our format: legacy plaintext row written before
		// encryption was added, or corrupt data. Fail open.
		return stored;
	}

	const auto &key = LoadOrCreateKey();

	const auto iv = reinterpret_cast<const unsigned char*>(stored.data());
	const auto tag = reinterpret_cast<const unsigned char*>(
		stored.data() + kIvSize);
	const auto cipher = reinterpret_cast<const unsigned char*>(
		stored.data() + kIvSize + kTagSize);
	const auto cipherLen = int(stored.size() - kIvSize - kTagSize);

	auto plaintext = std::vector<char>(cipherLen);

	auto ctx = CipherContext(EVP_CIPHER_CTX_new());
	if (!ctx) {
		return stored;
	}

	auto len = 0;
	auto plaintextLen = 0;
	auto ok = EVP_DecryptInit_ex(
			ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr)
		&& EVP_CIPHER_CTX_ctrl(
			ctx.get(), EVP_CTRL_GCM_SET_IVLEN, kIvSize, nullptr)
		&& EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr, key.data(), iv);
	if (ok && cipherLen > 0) {
		ok = EVP_DecryptUpdate(
			ctx.get(),
			reinterpret_cast<unsigned char*>(plaintext.data()),
			&len,
			cipher,
			cipherLen);
		plaintextLen = len;
	}
	if (ok) {
		ok = EVP_CIPHER_CTX_ctrl(
			ctx.get(),
			EVP_CTRL_GCM_SET_TAG,
			kTagSize,
			const_cast<unsigned char*>(tag));
	}
	if (ok) {
		ok = EVP_DecryptFinal_ex(
			ctx.get(),
			reinterpret_cast<unsigned char*>(plaintext.data())
				+ plaintextLen,
			&len);
		plaintextLen += len;
	}
	if (!ok) {
		// Authentication failed: wrong key, corrupted row, or (most likely
		// in practice) a legacy plaintext row that happens to be long
		// enough to look like our format. Fail open and hand back the
		// original bytes instead of losing the user's data.
		return stored;
	}

	plaintext.resize(plaintextLen);
	return plaintext;
}

std::string EncryptString(const std::string &plaintext) {
	if (plaintext.empty()) {
		return {};
	}
	const auto in = std::vector<char>(plaintext.begin(), plaintext.end());
	const auto out = EncryptBlob(in);
	return QByteArray(out.data(), int(out.size())).toBase64().toStdString();
}

std::string DecryptString(const std::string &stored) {
	if (stored.empty()) {
		return {};
	}
	const auto encoded = QByteArray::fromStdString(stored);
	const auto decoded = QByteArray::fromBase64(
		encoded,
		QByteArray::Base64Encoding
			| QByteArray::AbortOnBase64DecodingErrors);
	if (decoded.isEmpty()) {
		// Not valid base64 (or decodes to nothing meaningful): legacy
		// plaintext row written before encryption was added.
		return stored;
	}
	const auto in = std::vector<char>(
		decoded.constData(),
		decoded.constData() + decoded.size());
	const auto out = DecryptBlob(in);
	return std::string(out.begin(), out.end());
}

} // namespace AyuCrypto
