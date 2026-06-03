#include "ofApp.h"
#include "MacAsyncFileDialog.h"
#include "ofAppGLFWWindow.h"
#include "ofxJSON.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <unistd.h>

static uint32_t stableStringHash(const std::string & s) {
	uint32_t h = 2166136261u;
	for (unsigned char c : s) {
		h ^= c;
		h *= 16777619u;
	}
	return h;
}

static std::string pointGlyphModeName(PointGlyphMode mode) {
	switch (mode) {
	case PointGlyphMode::CIRCLE: return "CIRCLE";
	case PointGlyphMode::SQUARE: return "SQUARE";
	case PointGlyphMode::X_MARK: return "X";
	case PointGlyphMode::NUMBER: return "NUMBER";
	case PointGlyphMode::CLUSTER_NUMBER: return "CLUSTER";
	case PointGlyphMode::EMOJI: return "EMOJI";
	case PointGlyphMode::THUMBNAIL: return "THUMB";
	case PointGlyphMode::MIXED: return "MIXED";
	default: return "CIRCLE";
	}
}

static PointGlyphMode nextPointGlyphMode(PointGlyphMode mode) {
	int v = ((int)mode + 1) % 8;
	return (PointGlyphMode)v;
}

static std::string spectrogramBlendModeName(SpectrogramBlendMode mode) {
	switch (mode) {
	case SpectrogramBlendMode::NORMAL: return "NORMAL";
	case SpectrogramBlendMode::ADD: return "ADD";
	case SpectrogramBlendMode::MULTIPLY: return "MULTIPLY";
	case SpectrogramBlendMode::SCREEN: return "SCREEN";
	case SpectrogramBlendMode::LUMA_KEY: return "LUMA KEY";
	default: return "NORMAL";
	}
}

static SpectrogramBlendMode nextSpectrogramBlendMode(SpectrogramBlendMode mode) {
	int v = ((int)mode + 1) % 5;
	return (SpectrogramBlendMode)v;
}

static ofBlendMode ofBlendModeFromSpectrogramBlendMode(SpectrogramBlendMode mode) {
	switch (mode) {
	case SpectrogramBlendMode::ADD: return OF_BLENDMODE_ADD;
	case SpectrogramBlendMode::MULTIPLY: return OF_BLENDMODE_MULTIPLY;
	case SpectrogramBlendMode::SCREEN: return OF_BLENDMODE_SCREEN;
	case SpectrogramBlendMode::LUMA_KEY: return OF_BLENDMODE_ALPHA;
	case SpectrogramBlendMode::NORMAL:
	default:
		return OF_BLENDMODE_ALPHA;
	}
}

static std::string shellQuote(const std::string & s) {
	std::string out = "'";
	for (char c : s) {
		if (c == '\'') out += "'\\''";
		else out += c;
	}
	out += "'";
	return out;
}

static bool ffmpegAvailable() {
	static int cached = -1;
	if (cached >= 0) return cached == 1;

	auto isExec = [](const std::string & p) {
		return !p.empty() && access(p.c_str(), X_OK) == 0;
	};
	auto findExec = [&](const std::string & name) {
		const char * pathEnv = std::getenv("PATH");
		std::vector<std::string> dirs;
		if (pathEnv != nullptr) {
			std::stringstream ss(pathEnv);
			std::string item;
			while (std::getline(ss, item, ':')) {
				if (!item.empty()) dirs.push_back(item);
			}
		}
		dirs.push_back("/opt/homebrew/bin");
		dirs.push_back("/usr/local/bin");
		dirs.push_back("/usr/bin");
		dirs.push_back("/bin");
		for (const auto & d : dirs) {
			std::string p = d + "/" + name;
			if (isExec(p)) return p;
		}
		return std::string();
	};

	std::string ffmpegPath = findExec("ffmpeg");
	std::string ffprobePath = findExec("ffprobe");
	cached = (!ffmpegPath.empty() && !ffprobePath.empty()) ? 1 : 0;
	return cached == 1;
}

static std::string ffmpegExecutablePath() {
	auto isExec = [](const std::string & p) {
		return !p.empty() && access(p.c_str(), X_OK) == 0;
	};
	const char * pathEnv = std::getenv("PATH");
	std::vector<std::string> dirs;
	if (pathEnv != nullptr) {
		std::stringstream ss(pathEnv);
		std::string item;
		while (std::getline(ss, item, ':')) {
			if (!item.empty()) dirs.push_back(item);
		}
	}
	dirs.push_back("/opt/homebrew/bin");
	dirs.push_back("/usr/local/bin");
	dirs.push_back("/usr/bin");
	dirs.push_back("/bin");
	for (const auto & d : dirs) {
		std::string p = d + "/ffmpeg";
		if (isExec(p)) return p;
	}
	return "ffmpeg";
}

static std::string ffprobeExecutablePath() {
	auto isExec = [](const std::string & p) {
		return !p.empty() && access(p.c_str(), X_OK) == 0;
	};
	const char * pathEnv = std::getenv("PATH");
	std::vector<std::string> dirs;
	if (pathEnv != nullptr) {
		std::stringstream ss(pathEnv);
		std::string item;
		while (std::getline(ss, item, ':')) {
			if (!item.empty()) dirs.push_back(item);
		}
	}
	dirs.push_back("/opt/homebrew/bin");
	dirs.push_back("/usr/local/bin");
	dirs.push_back("/usr/bin");
	dirs.push_back("/bin");
	for (const auto & d : dirs) {
		std::string p = d + "/ffprobe";
		if (isExec(p)) return p;
	}
	return "ffprobe";
}

static double ffprobeDurationSeconds(const std::string & path) {
	if (!ffmpegAvailable()) return 0.0;
	std::string cmd = shellQuote(ffprobeExecutablePath())
		+ " -v error -show_entries format=duration -of default=nk=1:nw=1 "
		+ shellQuote(path);
	FILE * pipe = popen(cmd.c_str(), "r");
	if (!pipe) return 0.0;
	char buf[256];
	std::string out;
	while (fgets(buf, sizeof(buf), pipe) != nullptr) {
		out += buf;
	}
	pclose(pipe);
	try {
		return std::max(0.0, std::stod(out));
	} catch (...) {
		return 0.0;
	}
}

static const std::array<std::string, 12> kEmojiGlyphs = {
	"😀", "🔥", "✨", "🌊", "🌱", "🎵", "⚡", "🌀", "🌙", "☀", "🔷", "🍀"
};

static const std::array<std::string, 12> kEmojiFallbackGlyphs = {
	":)", "*", "+", "~", "^", "♪", "!", "@", "o", "O", "[]", "%"
};

// Natural Sort Helper
int naturalCompare(const std::string & s1, const std::string & s2) {
	size_t i = 0, j = 0;
	while (i < s1.length() && j < s2.length()) {
		char c1 = s1[i];
		char c2 = s2[j];

		if (isdigit(c1) && isdigit(c2)) {
			// Extract numbers
			std::string n1, n2;
			while (i < s1.length() && isdigit(s1[i]))
				n1 += s1[i++];
			while (j < s2.length() && isdigit(s2[j]))
				n2 += s2[j++];

			// Compare length then value
			if (n1.length() != n2.length()) return (int)(n1.length() - n2.length());
			if (n1 != n2) return n1 < n2 ? -1 : 1;
		} else {
			if (c1 != c2) return c1 - c2;
			i++;
			j++;
		}
	}
	return (int)(s1.length() - s2.length());
}

static std::string getParentDirectoryPath(const std::string & dirPath) {
	if (dirPath.empty()) return "";
	std::filesystem::path p(dirPath);
	p = p.lexically_normal();
	std::filesystem::path parent = p.parent_path();
	if (parent.empty() || parent == p) return "";
	std::string out = parent.string();
	if (!out.empty() && out.back() != '/') out += "/";
	return out;
}

std::string ofApp::findVideoSegmentPath(const std::string & baseName) const {
	if (baseName.empty() || mediaRoot.empty()) return "";
	std::string videoDir = mediaRoot + "video_segments/";
	if (!ofDirectory(videoDir).exists()) return "";

	static const std::vector<std::string> exts = {"mp4", "mov", "m4v", "avi", "webm"};
	for (const auto & ext : exts) {
		std::string p = videoDir + baseName + "." + ext;
		if (ofFile(p).exists()) return p;
	}
	return "";
}

bool ofApp::ensureImageSegmentForBaseName(const std::string & baseName) {
	if (baseName.empty() || mediaRoot.empty()) return false;

	std::string imageDir = mediaRoot + "image_segments/";
	std::string imagePath = imageDir + baseName + ".png";
	if (ofFile(imagePath).exists()) return true;

	std::string videoPath = findVideoSegmentPath(baseName);
	if (videoPath.empty()) return false;

	ofDirectory::createDirectory(imageDir, true, true);

	// Preferred path: ffmpeg extraction is far more reliable than AVFoundation
	// for offline frame capture on macOS.
	if (ffmpegAvailable()) {
		std::string ffmpegBin = ffmpegExecutablePath();
		double dur = ffprobeDurationSeconds(videoPath);
		double mid = (dur > 0.0) ? (dur * 0.5) : 0.0;
		double start = std::max(0.0, mid - 0.75);
		char startBuf[64];
		snprintf(startBuf, sizeof(startBuf), "%.3f", start);
		char midBuf[64];
		snprintf(midBuf, sizeof(midBuf), "%.3f", std::max(0.0, mid));

		std::string vf = "thumbnail=120,"
			"scale='if(gte(iw,ih),256,-2)':'if(gte(iw,ih),-2,256)'";
		std::string cmdAvg = shellQuote(ffmpegBin) + " -v error -y -ss " + std::string(startBuf)
			+ " -i " + shellQuote(videoPath)
			+ " -vf \"" + vf + "\" -frames:v 1 " + shellQuote(imagePath)
			+ " >/dev/null 2>&1";

		auto imageIsNonBlack = [&](const std::string & p) {
			ofImage chk;
			if (!chk.load(p) || !chk.isAllocated()) return false;
			ofPixels & px = chk.getPixels();
			if (!px.isAllocated() || px.getNumChannels() < 3) return false;
			size_t stride = (size_t)px.getNumChannels();
			size_t nPix = (size_t)px.getWidth() * (size_t)px.getHeight();
			double sum = 0.0;
			for (size_t i = 0; i < nPix; i += 16) {
				size_t o = i * stride;
				sum += 0.2126 * px[o + 0] + 0.7152 * px[o + 1] + 0.0722 * px[o + 2];
			}
			double cnt = std::max<size_t>(1, nPix / 16);
			return (sum / cnt) > 2.0;
		};

		if (std::system(cmdAvg.c_str()) == 0 && ofFile(imagePath).exists() && imageIsNonBlack(imagePath)) {
			return true;
		}

		std::string cmdMid = shellQuote(ffmpegBin) + " -v error -y -ss " + std::string(midBuf)
			+ " -i " + shellQuote(videoPath)
			+ " -vf \"scale='if(gte(iw,ih),256,-2)':'if(gte(iw,ih),-2,256)'\""
			+ " -frames:v 1 " + shellQuote(imagePath)
			+ " >/dev/null 2>&1";
		if (std::system(cmdMid.c_str()) == 0 && ofFile(imagePath).exists() && imageIsNonBlack(imagePath)) {
			return true;
		}

		// Extra fallback near start to avoid black fades in some media.
		std::string cmdEarly = shellQuote(ffmpegBin) + " -v error -y -ss 0.2"
			+ " -i " + shellQuote(videoPath)
			+ " -vf \"scale='if(gte(iw,ih),256,-2)':'if(gte(iw,ih),-2,256)'\""
			+ " -frames:v 1 " + shellQuote(imagePath)
			+ " >/dev/null 2>&1";
		if (std::system(cmdEarly.c_str()) == 0 && ofFile(imagePath).exists() && imageIsNonBlack(imagePath)) {
			return true;
		}
	}

	if (!thumbnailExtractor) return false;

	if (thumbnailExtractor->isLoaded()) {
		thumbnailExtractor->stop();
	}
	if (!thumbnailExtractor->load(videoPath)) return false;
	thumbnailExtractor->setLoopState(OF_LOOP_NONE);
	thumbnailExtractor->play();

	for (int i = 0; i < 8; ++i) thumbnailExtractor->update();

	float srcW = std::max(1.0f, thumbnailExtractor->getWidth());
	float srcH = std::max(1.0f, thumbnailExtractor->getHeight());
	int maxDim = 256;
	int outW = maxDim;
	int outH = maxDim;
	if (srcW >= srcH) {
		outW = maxDim;
		outH = std::max(1, (int)std::round((srcH / srcW) * (float)maxDim));
	} else {
		outH = maxDim;
		outW = std::max(1, (int)std::round((srcW / srcH) * (float)maxDim));
	}

	ofFbo sampleFbo;
	sampleFbo.allocate(outW, outH, GL_RGBA);

	static constexpr int kTemporalSamples = 12;
	std::vector<uint64_t> accum;
	int accumW = outW, accumH = outH, accumChannels = 3;
	int samplesUsed = 0;
	accum.assign((size_t)accumW * (size_t)accumH * (size_t)accumChannels, 0);
	ofPixels bestPx;
	double bestLuma = -1.0;

	for (int si = 0; si < kTemporalSamples; ++si) {
		float t = (kTemporalSamples == 1) ? 0.5f
			: (0.20f + 0.60f * ((float)si / (float)(kTemporalSamples - 1)));
		thumbnailExtractor->setPosition(std::clamp(t, 0.0f, 1.0f));

		for (int i = 0; i < 24; ++i) thumbnailExtractor->update();

		ofPixels px;
		sampleFbo.begin();
		ofClear(0, 0, 0, 255);
		ofSetColor(255, 255, 255, 255);
		thumbnailExtractor->draw(0, 0, outW, outH);
		sampleFbo.end();
		sampleFbo.readToPixels(px);
		if (!px.isAllocated()) continue;
		if (px.getWidth() != accumW || px.getHeight() != accumH || px.getNumChannels() < 3) {
			continue;
		}

		double sumLuma = 0.0;
		size_t pixCount = (size_t)accumW * (size_t)accumH;
		size_t stride = (size_t)px.getNumChannels();
		for (size_t pi = 0; pi < pixCount; pi += 16) {
			size_t src = pi * stride;
			sumLuma += (0.2126 * (double)px[src + 0]) + (0.7152 * (double)px[src + 1]) + (0.0722 * (double)px[src + 2]);
		}
		double sampleCount = std::max<size_t>(1, pixCount / 16);
		double meanLuma = sumLuma / sampleCount;
		if (meanLuma > bestLuma) {
			bestLuma = meanLuma;
			bestPx = px;
		}
		if (meanLuma < 1.0) continue;

		pixCount = (size_t)accumW * (size_t)accumH;
		for (size_t pi = 0; pi < pixCount; ++pi) {
			size_t src = pi * (size_t)px.getNumChannels();
			size_t dst = pi * 3;
			accum[dst + 0] += px[src + 0];
			accum[dst + 1] += px[src + 1];
			accum[dst + 2] += px[src + 2];
		}
		samplesUsed++;
	}

	thumbnailExtractor->stop();
	if (samplesUsed <= 0 && bestPx.isAllocated() && bestPx.getNumChannels() >= 3) {
		accum.assign((size_t)accumW * (size_t)accumH * 3, 0);
		size_t pixCount = (size_t)accumW * (size_t)accumH;
		for (size_t pi = 0; pi < pixCount; ++pi) {
			size_t src = pi * (size_t)bestPx.getNumChannels();
			size_t dst = pi * 3;
			accum[dst + 0] = bestPx[src + 0];
			accum[dst + 1] = bestPx[src + 1];
			accum[dst + 2] = bestPx[src + 2];
		}
		samplesUsed = 1;
	}
	if (samplesUsed <= 0 || accum.empty()) {
		// Hard fallback: one midpoint rendered capture with extended decode warmup.
		if (thumbnailExtractor->isLoaded()) thumbnailExtractor->stop();
		if (!thumbnailExtractor->load(videoPath)) return false;
		thumbnailExtractor->setLoopState(OF_LOOP_NONE);
		thumbnailExtractor->play();
		thumbnailExtractor->setPosition(0.5f);
		for (int i = 0; i < 64; ++i) thumbnailExtractor->update();
		ofPixels px;
		sampleFbo.begin();
		ofClear(0, 0, 0, 255);
		ofSetColor(255, 255, 255, 255);
		thumbnailExtractor->draw(0, 0, outW, outH);
		sampleFbo.end();
		sampleFbo.readToPixels(px);
		if (!px.isAllocated() || px.getNumChannels() < 3) {
			px = thumbnailExtractor->getPixels();
		}
		thumbnailExtractor->stop();
		if (!px.isAllocated() || px.getNumChannels() < 3) return false;

		accum.assign((size_t)accumW * (size_t)accumH * 3, 0);
		size_t pixCount = (size_t)accumW * (size_t)accumH;
		for (size_t pi = 0; pi < pixCount; ++pi) {
			size_t src = pi * (size_t)px.getNumChannels();
			size_t dst = pi * 3;
			accum[dst + 0] = px[src + 0];
			accum[dst + 1] = px[src + 1];
			accum[dst + 2] = px[src + 2];
		}
		samplesUsed = 1;
	}

	ofPixels avgPx;
	avgPx.allocate(accumW, accumH, OF_IMAGE_COLOR);
	size_t avgN = accum.size();
	for (size_t bi = 0; bi < avgN; ++bi) {
		avgPx[bi] = (unsigned char)std::clamp<uint64_t>(accum[bi] / (uint64_t)samplesUsed, 0, 255);
	}

	// Do not save obviously black fallback thumbnails.
	double sumLuma = 0.0;
	size_t nPix = (size_t)accumW * (size_t)accumH;
	for (size_t i = 0; i < nPix; i += 16) {
		size_t o = i * 3;
		sumLuma += 0.2126 * avgPx[o + 0] + 0.7152 * avgPx[o + 1] + 0.0722 * avgPx[o + 2];
	}
	double cnt = std::max<size_t>(1, nPix / 16);
	if ((sumLuma / cnt) <= 2.0) {
		return false;
	}

	ofImage img;
	img.setFromPixels(avgPx);

	return img.save(imagePath);
}

std::string ofApp::resolveThumbnailPathForPoint(const DataPoint & p) {
	if (p.filename.empty()) return "";

	auto memoIt = thumbnailPathByFilename.find(p.filename);
	if (memoIt != thumbnailPathByFilename.end()) {
		return memoIt->second;
	}

	std::vector<std::string> stillCandidates;
	auto pushStillCandidate = [&](const std::string & relOrAbs) {
		std::vector<std::string> expanded;
		if (relOrAbs.empty()) return;
		if (ofFilePath::isAbsolute(relOrAbs)) {
			expanded.push_back(relOrAbs);
		} else {
			if (!mediaRoot.empty()) expanded.push_back(mediaRoot + relOrAbs);
			expanded.push_back(relOrAbs);
		}
		stillCandidates.insert(stillCandidates.end(), expanded.begin(), expanded.end());
	};

	std::string baseName = ofFilePath::removeExt(p.filename);
	std::string ext = ofToLower(ofFilePath::getFileExt(p.filename));

	// Preferred pipeline: persistent midpoint frame cache in segments/image_segments.
	if (!baseName.empty() && !mediaRoot.empty()) {
		std::string imageSegPath = mediaRoot + "image_segments/" + baseName + ".png";
		if (ofFile(imageSegPath).exists()) {
			thumbnailPathByFilename[p.filename] = imageSegPath;
			return imageSegPath;
		} else if (allowAutoMediaGeneration && ensureImageSegmentForBaseName(baseName)) {
			thumbnailPathByFilename[p.filename] = imageSegPath;
			return imageSegPath;
		}
	}

	pushStillCandidate(p.filename);

	std::vector<std::string> stillExts = {"png", "jpg", "jpeg", "webp", "bmp"};
	for (const auto & e : stillExts) {
		pushStillCandidate(baseName + "." + e);
		pushStillCandidate("stills/" + baseName + "." + e);
		pushStillCandidate("thumbs/" + baseName + "." + e);
		pushStillCandidate("thumbnails/" + baseName + "." + e);
		pushStillCandidate("video_segments/" + baseName + "." + e);
	}

	if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "webp" || ext == "bmp") {
		pushStillCandidate(baseName + "." + ext);
	}

	std::unordered_set<std::string> seen;
	for (const auto & c : stillCandidates) {
		if (c.empty() || seen.find(c) != seen.end()) continue;
		seen.insert(c);
		if (ofFile(c).exists()) {
			thumbnailPathByFilename[p.filename] = c;
			return c;
		}
	}

	thumbnailPathByFilename[p.filename] = "";
	return "";
}

std::shared_ptr<ofImage> ofApp::getThumbnailCached(const std::string & path, int & loadsRemaining) {
	if (path.empty()) return nullptr;

	uint64_t nowMs = ofGetElapsedTimeMillis();
	auto it = thumbnailCache.find(path);
	if (it != thumbnailCache.end()) {
		thumbnailCacheLastUsed[path] = nowMs;
		return it->second;
	}

	if (loadsRemaining <= 0) return nullptr;

	auto img = std::make_shared<ofImage>();
	std::string ext = ofToLower(ofFilePath::getFileExt(path));
	bool isVideoPath = (ext == "mp4" || ext == "mov" || ext == "m4v" || ext == "avi" || ext == "webm");
	if (isVideoPath) {
		std::string baseName = ofFilePath::removeExt(ofFilePath::getFileName(path));
		if (baseName.empty()) return nullptr;
		std::string imagePath = mediaRoot + "image_segments/" + baseName + ".png";
		if (!ofFile(imagePath).exists()) {
			if (!allowAutoMediaGeneration || !ensureImageSegmentForBaseName(baseName)) return nullptr;
		}
		if (!img->load(imagePath)) return nullptr;
	} else {
		if (!img->load(path)) {
			return nullptr;
		}
	}

	loadsRemaining--;
	thumbnailCache[path] = img;
	thumbnailCacheLastUsed[path] = nowMs;
	pruneThumbnailCache();
	return img;
}

void ofApp::pruneThumbnailCache() {
	while (thumbnailCache.size() > thumbnailCacheBudget) {
		uint64_t oldestTs = std::numeric_limits<uint64_t>::max();
		std::string oldestKey;
		for (const auto & kv : thumbnailCacheLastUsed) {
			if (kv.second < oldestTs) {
				oldestTs = kv.second;
				oldestKey = kv.first;
			}
		}
		if (oldestKey.empty()) break;
		thumbnailCache.erase(oldestKey);
		thumbnailCacheLastUsed.erase(oldestKey);
	}
}

std::string ofApp::resolveSpectrogramPathForMedia(const std::string & mediaPath) {
	if (mediaPath.empty()) return "";

	auto memoIt = spectrogramPathByMedia.find(mediaPath);
	if (memoIt != spectrogramPathByMedia.end()) return memoIt->second;

	if (!ofFile(mediaPath).exists()) {
		spectrogramPathByMedia[mediaPath] = "";
		return "";
	}

	std::string baseName = ofFilePath::removeExt(ofFilePath::getFileName(mediaPath));
	if (baseName.empty()) {
		spectrogramPathByMedia[mediaPath] = "";
		return "";
	}

	std::string specDir;
	if (!mediaRoot.empty()) {
		specDir = mediaRoot + "spectrogram_segments/";
	} else {
		ofFile f(mediaPath);
		specDir = f.getEnclosingDirectory() + "spectrogram_segments/";
	}
	std::string imagePath = specDir + baseName + ".png";
	if (ofFile(imagePath).exists()) {
		spectrogramPathByMedia[mediaPath] = imagePath;
		return imagePath;
	}

	if (!allowAutoMediaGeneration) {
		spectrogramPathByMedia[mediaPath] = "";
		return "";
	}

	if (!ffmpegAvailable()) {
		spectrogramPathByMedia[mediaPath] = "";
		return "";
	}

	ofDirectory::createDirectory(specDir, true, true);
	std::string ffmpegBin = ffmpegExecutablePath();

	std::string vf1 = "showspectrumpic=s=1536x864:legend=disabled:color=intensity:scale=log";
	std::string cmd1 = shellQuote(ffmpegBin)
		+ " -v error -y -i " + shellQuote(mediaPath)
		+ " -lavfi \"" + vf1 + "\" -frames:v 1 " + shellQuote(imagePath)
		+ " >/dev/null 2>&1";

	if (std::system(cmd1.c_str()) != 0 || !ofFile(imagePath).exists()) {
		std::string vf2 = "showspectrumpic=s=1536x864:legend=disabled";
		std::string cmd2 = shellQuote(ffmpegBin)
			+ " -v error -y -i " + shellQuote(mediaPath)
			+ " -lavfi \"" + vf2 + "\" -frames:v 1 " + shellQuote(imagePath)
			+ " >/dev/null 2>&1";
		if (std::system(cmd2.c_str()) != 0 || !ofFile(imagePath).exists()) {
			spectrogramPathByMedia[mediaPath] = "";
			return "";
		}
	}

	spectrogramPathByMedia[mediaPath] = imagePath;
	return imagePath;
}

std::shared_ptr<ofImage> ofApp::getSpectrogramCached(const std::string & imagePath) {
	if (imagePath.empty()) return nullptr;
	auto it = spectrogramCache.find(imagePath);
	if (it != spectrogramCache.end()) return it->second;
	auto img = std::make_shared<ofImage>();
	if (!img->load(imagePath) || !img->isAllocated()) return nullptr;
	spectrogramCache[imagePath] = img;
	return img;
}

std::shared_ptr<ofImage> ofApp::getSpectrogramDisplayCached(const std::string & imagePath) {
	if (spectrogramBlendMode != SpectrogramBlendMode::LUMA_KEY) {
		return getSpectrogramCached(imagePath);
	}

	int thresholdBucket = std::clamp((int)std::round(spectrogramLumaKeyThreshold * 255.0f), 0, 242);
	std::string cacheKey = imagePath + "|lk|" + ofToString(thresholdBucket);
	auto it = spectrogramDisplayCache.find(cacheKey);
	if (it != spectrogramDisplayCache.end()) return it->second;

	auto srcImg = getSpectrogramCached(imagePath);
	if (!srcImg || !srcImg->isAllocated()) return nullptr;

	const ofPixels & src = srcImg->getPixels();
	if (!src.isAllocated() || src.getNumChannels() < 3) return srcImg;

	ofPixels keyed;
	keyed.allocate(src.getWidth(), src.getHeight(), OF_IMAGE_COLOR_ALPHA);
	float threshold = (float)thresholdBucket;
	float denom = std::max(1.0f, 255.0f - threshold);
	int srcChannels = src.getNumChannels();
	for (int y = 0; y < src.getHeight(); ++y) {
		for (int x = 0; x < src.getWidth(); ++x) {
			size_t srcIndex = ((size_t)y * (size_t)src.getWidth() + (size_t)x) * (size_t)srcChannels;
			size_t dstIndex = ((size_t)y * (size_t)src.getWidth() + (size_t)x) * 4u;
			unsigned char r = src[srcIndex + 0];
			unsigned char g = src[srcIndex + 1];
			unsigned char b = src[srcIndex + 2];
			float luma = (0.2126f * (float)r) + (0.7152f * (float)g) + (0.0722f * (float)b);
			float alphaNorm = std::clamp((luma - threshold) / denom, 0.0f, 1.0f);
			keyed[dstIndex + 0] = r;
			keyed[dstIndex + 1] = g;
			keyed[dstIndex + 2] = b;
			keyed[dstIndex + 3] = (unsigned char)std::round(alphaNorm * 255.0f);
		}
	}

	auto keyedImg = std::make_shared<ofImage>();
	keyedImg->setFromPixels(keyed);
	spectrogramDisplayCache[cacheKey] = keyedImg;
	return keyedImg;
}

void ofApp::noteTriggeredMediaForSpectrogram(const std::string & mediaPath) {
	if (!spectrogramLayerEnabled) return;
	std::string imagePath = resolveSpectrogramPathForMedia(mediaPath);
	if (imagePath.empty()) return;
	currentSpectrogramImagePath = imagePath;
	getSpectrogramCached(imagePath);

	spectrogramTrailImagePaths.push_back(imagePath);
	int keepN = std::max(1, spectrogramTrailLength);
	while ((int)spectrogramTrailImagePaths.size() > keepN) {
		spectrogramTrailImagePaths.pop_front();
	}
}

void ofApp::setMediaGenerationStatus(const std::string & status, uint64_t holdMs) {
	mediaGenerationStatusText = status;
	mediaGenerationStatusUntilMs = ofGetElapsedTimeMillis() + holdMs;
}

void ofApp::beginMediaAssetGeneration() {
	if (mediaGenerationActive || mediaRoot.empty() || points.empty()) {
		if (mediaGenerationActive) {
			setMediaGenerationStatus("Media generation already running...", 1200);
		} else if (mediaRoot.empty()) {
			setMediaGenerationStatus("Cannot generate media: no media root loaded.", 1800);
		} else if (points.empty()) {
			setMediaGenerationStatus("Cannot generate media: no points loaded.", 1800);
		}
		return;
	}

	thumbnailPathByFilename.clear();
	spectrogramPathByMedia.clear();
	spectrogramCache.clear();
	spectrogramDisplayCache.clear();

	std::unordered_set<std::string> uniqueBaseNames;
	std::unordered_set<std::string> uniqueMediaPaths;
	for (const auto & p : points) {
		if (p.filename.empty()) continue;
		std::string baseName = ofFilePath::removeExt(p.filename);
		if (!baseName.empty()) uniqueBaseNames.insert(baseName);
		uniqueMediaPaths.insert(mediaRoot + p.filename);
	}

	pendingThumbnailBaseNames.clear();
	pendingSpectrogramMediaPaths.clear();
	for (const auto & baseName : uniqueBaseNames) {
		std::string imageSegPath = mediaRoot + "image_segments/" + baseName + ".png";
		if (!ofFile(imageSegPath).exists()) {
			pendingThumbnailBaseNames.push_back(baseName);
		}
	}
	for (const auto & mediaPath : uniqueMediaPaths) {
		std::string baseName = ofFilePath::removeExt(ofFilePath::getFileName(mediaPath));
		if (baseName.empty()) continue;
		std::string specPath = mediaRoot + "spectrogram_segments/" + baseName + ".png";
		if (!ofFile(specPath).exists()) {
			pendingSpectrogramMediaPaths.push_back(mediaPath);
		}
	}

	mediaGenerationTotal = (int)pendingThumbnailBaseNames.size() + (int)pendingSpectrogramMediaPaths.size();
	mediaGenerationDone = 0;
	mediaGenerationThumbGenerated = 0;
	mediaGenerationSpecGenerated = 0;

	if (mediaGenerationTotal <= 0) {
		setMediaGenerationStatus("All thumbnails and spectrograms are already present.", 1800);
		return;
	}

	mediaGenerationActive = true;
	setMediaGenerationStatus(
		"Generating media assets... 0/" + ofToString(mediaGenerationTotal),
		600000);
}

void ofApp::processMediaAssetGenerationStep() {
	if (!mediaGenerationActive) return;

	allowAutoMediaGeneration = true;

	if (!pendingThumbnailBaseNames.empty()) {
		std::string baseName = pendingThumbnailBaseNames.front();
		pendingThumbnailBaseNames.pop_front();
		if (ensureImageSegmentForBaseName(baseName)) {
			mediaGenerationThumbGenerated++;
		}
		mediaGenerationDone++;
	} else if (!pendingSpectrogramMediaPaths.empty()) {
		std::string mediaPath = pendingSpectrogramMediaPaths.front();
		pendingSpectrogramMediaPaths.pop_front();
		if (!resolveSpectrogramPathForMedia(mediaPath).empty()) {
			mediaGenerationSpecGenerated++;
		}
		mediaGenerationDone++;
	}

	allowAutoMediaGeneration = false;

	if (mediaGenerationDone < mediaGenerationTotal) {
		setMediaGenerationStatus(
			"Generating media assets... " + ofToString(mediaGenerationDone) + "/" + ofToString(mediaGenerationTotal)
			+ " (thumb " + ofToString(mediaGenerationThumbGenerated)
			+ ", spec " + ofToString(mediaGenerationSpecGenerated) + ")",
			600000);
	} else {
		mediaGenerationActive = false;
		thumbnailPathByFilename.clear();
		spectrogramPathByMedia.clear();
		spectrogramCache.clear();
		spectrogramDisplayCache.clear();
		setMediaGenerationStatus(
			"Media generation complete. thumbs=" + ofToString(mediaGenerationThumbGenerated)
			+ " specs=" + ofToString(mediaGenerationSpecGenerated),
			3500);
		ofLogNotice("ofApp") << mediaGenerationStatusText;
	}
}

//--------------------------------------------------------------
void ofApp::setup() {
	ofSetBackgroundColor(20, 25, 40);
	ofSetFrameRate(60);
	ofSetEscapeQuitsApp(false);

	// Initialize OSC
	// SC: 57120, UI: 57122, Listen: 57121
	oscManager.setup("127.0.0.1", 57120, 57122, 57121);

	// Bind Projector Window if available
	if (projectorWindow) {
		ofAddListener(projectorWindow->events().draw, this, &ofApp::drawProjector);
	}

	// Send clear
	oscManager.sendClear();
	oscManager.sendUIPathRemove("all");

	// Initialize state
	points.clear();
	paths.clear();
	thumbnailCache.clear();
	thumbnailCacheLastUsed.clear();
	thumbnailPathByFilename.clear();
	spectrogramCache.clear();
	spectrogramDisplayCache.clear();
	spectrogramPathByMedia.clear();
	currentSpectrogramImagePath.clear();
	spectrogramTrailImagePaths.clear();
	thumbnailExtractor = std::make_shared<ofVideoPlayer>();
	thumbnailExtractor->setUseTexture(true);

	// Initialize default path 0 for settings
	auto p0 = std::make_shared<PathObject>(0);
	p0->name = "path-0";
	p0->mode = defaultPathMode;
	p0->isActive = false; // Not playing by itself
	p0->sendToVideo = false; // Don't trigger videos for browsing/hovering by default
	// User requested path-0 notification
	oscManager.sendUIPathAdd(p0->name);
	paths.push_back(p0);

	pathIdCounter = 1;
	currentMode = NAVIGATE;
	isDrawingPath = false;
	zoom = 1.0f;
	pan.set(0, 0);
	targetZoom = zoom;
	targetPan = pan;
	isViewAnimating = false;
	isDragging = false;
	isMarqueeZooming = false;
	isRecordingGesture = false;
	isDraggingPath = false;
	showVideo = false;
	showText = false;
	showTitle = false;
	compositionTitle = "";
	showGui = false;
	showHelp = false;
	lastVideoSwitchTime = 0;
	videoFront = &_vp1;
	videoBack = &_vp2;
	videoHoldAllocated = false;
	videoAlpha = 255.0f;
	videoAlphaTarget = 255.0f;
	videoFadeSpeed = 15.0f;

	// GUI Setup
	params.setName("Visual Settings");
	params.add(backgroundColor.set("Background", ofColor(20, 25, 40), ofColor(0, 0), ofColor(255, 255)));
	params.add(pointColor.set("Point Color", ofColor(100, 150, 255), ofColor(0, 0), ofColor(255, 255)));
	params.add(selectedColor.set("Selected Color", ofColor(255, 100, 100), ofColor(0, 0), ofColor(255, 255)));
	params.add(hoveredColor.set("Hovered Color", ofColor(255, 255, 100), ofColor(0, 0), ofColor(255, 255)));
	params.add(pathColor.set("Path Color", ofColor(255, 0, 144), ofColor(0, 0), ofColor(255, 255)));
	params.add(selectedPathColor.set("Active Path Color", ofColor(255, 255, 0), ofColor(0, 0), ofColor(255, 255)));
	params.add(activePointColor.set("Active Point Color", ofColor(255, 255, 255), ofColor(0, 0), ofColor(255, 255)));
	params.add(textColor.set("Text Color", ofColor(255), ofColor(0, 0), ofColor(255, 255)));
	params.add(activeTextColor.set("Active Text Color", ofColor(0, 255, 0), ofColor(0, 0), ofColor(255, 255)));
	params.add(titleColor.set("Title Color", ofColor(255, 255, 255), ofColor(0, 0), ofColor(255, 255)));
	params.add(debugTextColor.set("Debug Text Color", ofColor(255), ofColor(0, 0), ofColor(255, 255)));

	params.add(pointSize.set("Point Size", 5.0f, 1.0f, 100.0f)); // Increased default to 5.0
	params.add(selectedPointSize.set("Sel Point Size", 8.0f, 1.0f, 30.0f));
	params.add(hoveredPointSize.set("Hover Point Size", 16.0f, 1.0f, 40.0f));
	params.add(fontSize.set("Font Size", 14.0f, 8.0f, 48.0f));
	params.add(annotationFontSize.set("Annotation Font Size", 14.0f, 8.0f, 48.0f));
	params.add(activeFontSize.set("Active Font Size", 14.0f, 8.0f, 48.0f));
	params.add(titleFontSize.set("Title Font Size", 48.0f, 12.0f, 120.0f));
	params.add(gridColor.set("Grid Color", ofColor(120, 140, 170, 70), ofColor(0, 0, 0, 0), ofColor(255, 255, 255, 255)));
	params.add(gridSpacing.set("Grid Spacing", 2.0f, 0.2f, 20.0f));
	params.add(zoomAnimationSpeed.set("Zoom Animation Speed", 0.2f, 0.02f, 1.0f));
	params.add(playheadSize.set("Playhead Size", 1.0f, 0.1f, 200.0f));
	params.add(pathThickness.set("Path Thickness", 1.0f, 0.1f, 20.0f));
	params.add(selectedPathThickness.set("Selected Path Thickness", 2.0f, 0.1f, 20.0f));
	params.add(playheadColor.set("Playhead Color", ofColor(255), ofColor(0, 0), ofColor(255, 255)));
	params.add(videoFitMode.set("Video Fit 0=stretch 1=height 2=width", 0, 0, 2));
	params.add(videoDisplayMode.set("Video Mode 0=single 1=grid 2=blendmix 3=mapped 4=collage 5=mosaic", 0, 0, 5));
	params.add(mosaicReplaceRatio.set("Mosaic Replace Ratio", 0.75f, 0.0f, 1.0f));
	params.add(pointGlyphMode_param.set("Point Glyph 0=O 1=[] 2=X 3=# 4=cluster 5=emoji 6=thumb 7=mix", 0, 0, 7));
	params.add(spectrogramLayerAlpha_param.set("Spectrogram Alpha", spectrogramLayerAlpha, 0.0f, 1.0f));
	params.add(spectrogramTrailAlpha_param.set("Spectrogram Trail Alpha", spectrogramTrailAlpha, 0.0f, 1.0f));
	params.add(spectrogramTrailLength_param.set("Spectrogram Trail Length", spectrogramTrailLength, 1, 24));
	params.add(spectrogramLumaKeyThreshold_param.set("Spectrogram Luma Key", spectrogramLumaKeyThreshold, 0.0f, 0.95f));
	params.add(spectrogramBlendMode_param.set("Spectrogram Blend 0=normal 1=add 2=multiply 3=screen 4=luma", (int)spectrogramBlendMode, 0, 4));
	params.add(videoFadeSpeed_param.set("Video Fade Speed", 15.0f, 1.0f, 60.0f));
	params.add(cloudTransitionSpeed.set("Cloud Transition Speed", 0.05f, 0.01f, 1.0f));
	params.add(neighbourSeqGapMs_param.set("Neighbour Seq Gap (ms)", 300.0f, 50.0f, 2000.0f));
	params.add(audioVisualFeedbackEnabled.set("Audio Visual Feedback", true));
	params.add(audioEnergyVisualAmount.set("Audio Energy Amount", 0.35f, 0.0f, 1.0f));
	params.add(audioOnsetVisualAmount.set("Audio Onset Amount", 0.25f, 0.0f, 1.0f));
	params.add(audioPathWobbleAmount.set("Audio Path Wobble", 0.15f, 0.0f, 1.0f));

	// Initialize Grid mode layers
	for (int i = 0; i < (GRID_COLS * GRID_ROWS); i++) {
		gridPlayers.push_back(std::make_shared<ofVideoPlayer>());
	}

	// Initialize Ghost mode pool
	for (int i = 0; i < kGhostLayers; i++) {
		ghostPlayers.push_back(std::make_shared<ofVideoPlayer>());
	}

	// Initialize Mapped mode pool (reused to avoid runtime player churn)
	for (int i = 0; i < kMappedMax; i++) {
		MappedClip mc;
		mc.player = std::make_shared<ofVideoPlayer>();
		mappedPlayers.push_back(mc);
	}

	// Initialize Tile Collage mode layers
	int collageCount = kCollageCols * kCollageRows;
	collagePlayers.reserve(collageCount);
	collageAnglesDeg.assign(collageCount, 0.0f);
	collageOffsetsPx.assign(collageCount, ofVec2f(0, 0));
	for (int i = 0; i < collageCount; i++) {
		collagePlayers.push_back(std::make_shared<ofVideoPlayer>());
	}

	// Initialize Tile Mosaic mode (mode 5)
	{
		mosaicPool.reserve(kMosaicMaxHistory);
		for (int i = 0; i < kMosaicMaxHistory; i++) {
			mosaicPool.push_back(std::make_shared<ofVideoPlayer>());
		}
		mosaicHoldFbos.resize(kMosaicMaxHistory);
		mosaicHoldAllocated.assign(kMosaicMaxHistory, false);
		mosaicTileAssignment.assign(kMosaicCols * kMosaicRows, -1);
	}

	gui.setup(params);
	gui.loadFromFile("settings.xml");

	// Load Font - Latin Modern Roman (LaTeX-style serif)
	bool fontLoaded = font.load("lmroman10-regular.otf", fontSize);
	if (!fontLoaded) fontLoaded = font.load(OF_TTF_SANS, fontSize);
	//bool annotationFontLoaded = annotationFont.load("lmroman10-italic.otf", annotationFontSize);
	bool annotationFontLoaded = annotationFont.load("lmroman10-regular.otf", annotationFontSize);
	if (!annotationFontLoaded) annotationFontLoaded = annotationFont.load(OF_TTF_SANS, annotationFontSize);
	if (annotationFontLoaded) {
		ofRectangle emojiBox = annotationFont.getStringBoundingBox("😀", 0.0f, 0.0f);
		ofRectangle fallbackBox = annotationFont.getStringBoundingBox("?", 0.0f, 0.0f);
		emojiGlyphLikelySupported = (emojiBox.width > 0.0f && emojiBox.height > 0.0f
			&& std::abs(emojiBox.width - fallbackBox.width) > 0.5f);
	}

	bool activeFontLoaded = activeFont.load("lmroman10-regular.otf", activeFontSize);
	if (!activeFontLoaded) activeFontLoaded = activeFont.load(OF_TTF_SANS, activeFontSize);

	bool titleFontLoaded = titleFont.load("lmroman10-bold.otf", titleFontSize);
	if (!titleFontLoaded) titleFontLoaded = titleFont.load(OF_TTF_SANS, titleFontSize);

	// Load default data
	// In a real app we'd use a file dialog or settings file
	// For now, let's look for a json file in bin/data
	/*
	ofDirectory dir("");
	dir.allowExt("json");
	dir.listDir();
	if (dir.size() > 0) {
		loadPoints(dir.getPath(0));
	}
	*/

	// Annotation system — loadFromFile is deferred to loadPoints() so that
	// annotations are never drawn against an empty point cloud on startup.
	annotationManager.init(&zoom, &pan, &annotationFont);
}

//--------------------------------------------------------------
void ofApp::update() {
	// Fade point/path visuals independently from video playback.
	float fadeStep = ofGetLastFrameTime() / 0.1f;
	if (dataVisualAlpha < targetDataVisualAlpha) {
		dataVisualAlpha = std::min(dataVisualAlpha + fadeStep, targetDataVisualAlpha);
	} else if (dataVisualAlpha > targetDataVisualAlpha) {
		dataVisualAlpha = std::max(dataVisualAlpha - fadeStep, targetDataVisualAlpha);
	}

	// Run background media generation in small steps so UI stays responsive.
	processMediaAssetGenerationStep();

	// Smoothly animate zoom/pan towards targets.
	if (isViewAnimating) {
		float k = std::clamp((float)zoomAnimationSpeed.get(), 0.0f, 1.0f);
		zoom = ofLerp(zoom, targetZoom, k);
		pan = pan.getInterpolated(targetPan, k);

		if (std::abs(zoom - targetZoom) < 0.0005f && pan.squareDistance(targetPan) < 0.01f) {
			zoom = targetZoom;
			pan = targetPan;
			isViewAnimating = false;
		}
	}

	// Poll OSC Messages
	ofxOscMessage m;
	while (oscManager.getNextMessage(m)) {
		std::string addr = m.getAddress();

		// Find "selected" path or default to path-0
		// Processing logic: "pathToUse = browsePath; if (selectedPath != null) pathToUse = selectedPath;"
		// For us, let's use the last selected path or path-0
		auto pathToUse = paths[0];
		for (auto & p : paths) {
			if (p->isSelected) {
				pathToUse = p;
				break;
			}
		}

		if (addr == "/o_playPause") {
			pathToUse->isActive = !pathToUse->isActive;
			if (!pathToUse->isActive) {
				stopPathSamples(pathToUse);
			}
			oscManager.sendUIPathUpdate(pathToUse->id, pathToUse->isActive, pathToUse->radius, pathToUse->direction, pathToUse->sampleNum, pathToUse->volume, pathToUse->falloff, pathToUse->speed, pathToUse->mode);

		} else if (addr == "/engineready") {
			if (m.getNumArgs() > 0 && (m.getArgType(0) == OFXOSC_TYPE_INT32 || m.getArgType(0) == OFXOSC_TYPE_INT64)) {
				int scPort = m.getArgAsInt32(0);
				oscManager.setSCPort(scPort);
				ofLogNotice("OSC") << "SC engine ready, using incoming lang port: " << scPort;
			} else {
				ofLogNotice("OSC") << "SC engine ready (no port arg), keeping SC port: " << oscManager.getSCPort();
			}

		} else if (addr == "/o_path") {
			// Select path by name or ID?
			// Processing: String pathName = msg.get(0).stringValue();
			if (m.getArgType(0) == OFXOSC_TYPE_STRING) {
				std::string pathName = m.getArgAsString(0);
				// Deselect all
				for (auto & p : paths)
					p->isSelected = false;
				// Select matching
				for (auto & p : paths) {
					if (p->name == pathName) {
						p->isSelected = true;
						// Send full feedback with all params including effects/pulser
						sendFullUIUpdate(p);
						break;
					}
				}
			} else if (m.getArgType(0) == OFXOSC_TYPE_INT32) {
				int id = m.getArgAsInt32(0);
				for (auto & p : paths)
					p->isSelected = false;
				for (auto & p : paths) {
					if (p->id == id) {
						p->isSelected = true;
						sendFullUIUpdate(p);
						break;
					}
				}
			}

		} else if (addr == "/o_synth") {
			// Mode int constants from Processing: CLOUD=1, LOOP=2, ONCE=3, MIXED=9
			pathToUse->mode = m.getArgAsInt32(0);
		} else if (addr == "/o_direction") {
			pathToUse->direction = m.getArgAsInt32(0);
		} else if (addr == "/o_radius") {
			pathToUse->radius = m.getArgAsFloat(0);
		} else if (addr == "/o_speed") {
			pathToUse->speed = m.getArgAsFloat(0);
		} else if (addr == "/o_samples") {
			pathToUse->sampleNum = m.getArgAsInt32(0);
		} else if (addr == "/o_amp") {
			pathToUse->volume = m.getArgAsFloat(0);
			oscManager.sendPathVolume(pathToUse->name, pathToUse->volume);
		} else if (addr == "/o_falloff") {
			pathToUse->falloff = m.getArgAsFloat(0);
		}
		// Granular params
		else if (addr == "/o_gRate") {
			pathToUse->gRateMin = m.getArgAsFloat(0);
			pathToUse->gRateMax = m.getArgAsFloat(1);
		} else if (addr == "/o_gDur") {
			pathToUse->gDurMin = m.getArgAsFloat(0);
			pathToUse->gDurMax = m.getArgAsFloat(1);
		} else if (addr == "/o_gGrainRate") {
			pathToUse->gGrainRateMin = m.getArgAsInt32(0);
			pathToUse->gGrainRateMax = m.getArgAsInt32(1);
		} else if (addr == "/o_gPos") {
			pathToUse->gPosMin = m.getArgAsFloat(0);
			pathToUse->gPosMax = m.getArgAsFloat(1);
		} else if (addr == "/o_gRand") {
			pathToUse->gRand = m.getArgAsFloat(0);
		} else if (addr == "/o_grainAlgo") {
			pathToUse->granularMode = m.getArgAsInt32(0);
		} else if (addr == "/o_grainEnv") {
			pathToUse->gEnv = m.getArgAsInt32(0);
		}

		// ---- Effects (prefix /o_eff) ----
		else if (addr.rfind("/o_eff", 0) == 0) {
			ofxOscMessage fx;

			// --- Reverb ---
			if (addr.rfind("/o_effReverb", 0) == 0) {
				if (addr == "/o_effReverb" && m.getArgAsInt32(0) == 0) {
					pathToUse->fxEnabled[0] = false;
					oscManager.sendPathRemoveEffect(pathToUse->name, "reverb");
				} else {
					pathToUse->fxEnabled[0] = true;
					if (addr == "/o_effReverbMix")
						pathToUse->fxReverb[0] = m.getArgAsFloat(0);
					else if (addr == "/o_effReverbRoom")
						pathToUse->fxReverb[1] = m.getArgAsFloat(0);
					else if (addr == "/o_effReverbDamp")
						pathToUse->fxReverb[2] = m.getArgAsFloat(0);
					fx.setAddress("/pathreverb");
					fx.addStringArg(pathToUse->name);
					fx.addFloatArg(pathToUse->fxReverb[0]);
					fx.addFloatArg(pathToUse->fxReverb[1]);
					fx.addFloatArg(pathToUse->fxReverb[2]);
					oscManager.sendMessage(fx);
				}
			}

			// --- Delay ---
			else if (addr.rfind("/o_effDelay", 0) == 0) {
				if (addr == "/o_effDelay" && m.getArgAsInt32(0) == 0) {
					pathToUse->fxEnabled[1] = false;
					oscManager.sendPathRemoveEffect(pathToUse->name, "delay");
				} else {
					pathToUse->fxEnabled[1] = true;
					// Processing stores 2x the UI value for delay time
					if (addr == "/o_effDelayTime")
						pathToUse->fxDelay[0] = 2.0f * m.getArgAsFloat(0);
					else if (addr == "/o_effDelayFeedback")
						pathToUse->fxDelay[1] = m.getArgAsFloat(0);
					else if (addr == "/o_effDelayMix")
						pathToUse->fxDelay[2] = m.getArgAsFloat(0);
					fx.setAddress("/pathdelay");
					fx.addStringArg(pathToUse->name);
					fx.addFloatArg(pathToUse->fxDelay[0]);
					fx.addFloatArg(pathToUse->fxDelay[1]);
					fx.addFloatArg(pathToUse->fxDelay[2]);
					oscManager.sendMessage(fx);
				}
			}

			// --- Distortion ---
			else if (addr.rfind("/o_effDistortion", 0) == 0) {
				if (addr == "/o_effDistortion" && m.getArgAsInt32(0) == 0) {
					pathToUse->fxEnabled[2] = false;
					oscManager.sendPathRemoveEffect(pathToUse->name, "distortion");
				} else {
					pathToUse->fxEnabled[2] = true;
					if (addr == "/o_effDistortionDrive")
						pathToUse->fxDistortion[0] = m.getArgAsFloat(0);
					else if (addr == "/o_effDistortionMix")
						pathToUse->fxDistortion[1] = m.getArgAsFloat(0);
					fx.setAddress("/pathdistortion");
					fx.addStringArg(pathToUse->name);
					fx.addFloatArg(pathToUse->fxDistortion[0]);
					fx.addFloatArg(pathToUse->fxDistortion[1]);
					oscManager.sendMessage(fx);
				}
			}

			// --- Filter ---
			else if (addr.rfind("/o_effFilter", 0) == 0) {
				if (addr == "/o_effFilter" && m.getArgAsInt32(0) == 0) {
					pathToUse->fxEnabled[4] = false;
					oscManager.sendPathRemoveEffect(pathToUse->name, "filter");
				} else {
					pathToUse->fxEnabled[4] = true;
					if (addr == "/o_effFilterFreq")
						pathToUse->fxFilter[0] = m.getArgAsFloat(0);
					else if (addr == "/o_effFilterRes")
						pathToUse->fxFilter[1] = m.getArgAsFloat(0);
					else if (addr == "/o_effFilterType")
						pathToUse->fxFilter[2] = (float)m.getArgAsInt32(0);
					static const std::string filterTypes[] = { "lpf", "hpf", "bpf" };
					int typeIdx = (int)pathToUse->fxFilter[2];
					if (typeIdx < 0) typeIdx = 0;
					if (typeIdx > 2) typeIdx = 2;
					fx.setAddress("/pathfilter");
					fx.addStringArg(pathToUse->name);
					fx.addFloatArg(pathToUse->fxFilter[0]);
					fx.addFloatArg(pathToUse->fxFilter[1]);
					fx.addStringArg(filterTypes[typeIdx]);
					oscManager.sendMessage(fx);
				}
			}

			// --- Compressor ---
			else if (addr.rfind("/o_effCompressor", 0) == 0) {
				if (addr == "/o_effCompressor" && m.getArgAsInt32(0) == 0) {
					pathToUse->fxEnabled[3] = false;
					oscManager.sendPathRemoveEffect(pathToUse->name, "compressor");
				} else {
					pathToUse->fxEnabled[3] = true;
					// Processing stores 2x the UI value for threshold
					if (addr == "/o_effCompressorThresh")
						pathToUse->fxCompressor[0] = 2.0f * m.getArgAsFloat(0);
					else if (addr == "/o_effCompressorRatio")
						pathToUse->fxCompressor[1] = m.getArgAsFloat(0);
					else if (addr == "/o_effCompressorAttack")
						pathToUse->fxCompressor[2] = m.getArgAsFloat(0);
					else if (addr == "/o_effCompressorRelease")
						pathToUse->fxCompressor[3] = m.getArgAsFloat(0);
					fx.setAddress("/pathcompressor");
					fx.addStringArg(pathToUse->name);
					fx.addFloatArg(pathToUse->fxCompressor[0]);
					fx.addFloatArg(pathToUse->fxCompressor[1]);
					fx.addFloatArg(pathToUse->fxCompressor[2]);
					fx.addFloatArg(pathToUse->fxCompressor[3]);
					oscManager.sendMessage(fx);
				}
			}
		}

		// ---- Pulser (prefix /o_pulser) ----
		else if (addr.rfind("/o_pulser", 0) == 0) {
			if (addr == "/o_pulserMix")
				pathToUse->pulserMix = m.getArgAsFloat(0);
			else if (addr == "/o_pulserRate") {
				pathToUse->pulserRateMin = m.getArgAsFloat(0);
				pathToUse->pulserRateMax = m.getArgAsFloat(1);
			} else if (addr == "/o_pulserRateRand")
				pathToUse->pulserRateRand = m.getArgAsFloat(0);
			else if (addr == "/o_pulserAttack")
				pathToUse->pulserAttack = m.getArgAsFloat(0);
			else if (addr == "/o_pulserRelease")
				pathToUse->pulserRelease = m.getArgAsFloat(0);

			// Always forward updated pulser state to SuperCollider
			oscManager.sendUpdatePulser(
				pathToUse->name,
				pathToUse->pulserMix,
				pathToUse->pulserRateMin,
				pathToUse->pulserRateMax,
				pathToUse->pulserRateRand,
				pathToUse->pulserAttack,
				pathToUse->pulserRelease);
		}

		// ---- Jitter toggle ----
		else if (addr == "/o_jitter") {
			pathToUse->jitterMode = (m.getArgAsInt32(0) != 0);
			if (!pathToUse->jitterMode) {
				// Sync lastJitterStepFloor so normal mode resumes cleanly
				pathToUse->lastJitterStepFloor = -1;
			}
		}

		// ---- Audio feedback: onset trigger ----
		else if (addr == "/o_onset") {
			lastOnsetTimeMs = ofGetElapsedTimeMillis();
		}

		// ---- Audio feedback: energy level ----
		else if (addr == "/o_energy") {
			audioEnergy = ofClamp(m.getArgAsFloat(0), 0.0f, 1.0f);
		}

		// ---- Audio feedback visual toggle ----
		else if (addr == "/o_visualfeedback") {
			audioVisualFeedbackEnabled = (m.getArgAsInt32(0) != 0);
		}
	}

	// Cloud transition interpolation
	bool pointsMoved = false;
	for (auto & p : points) {
		ofVec2f target;
		switch (currentCloudMode) {
		case PointCloudMode::LOCAL:
			target = p.pos_local;
			break;
		case PointCloudMode::MID:
			target = p.pos_mid;
			break;
		case PointCloudMode::GLOBAL:
			target = p.pos_global;
			break;
		}

		if (std::abs(p.x - target.x) > 0.001f || std::abs(p.y - target.y) > 0.001f) {
			p.x = ofLerp(p.x, target.x, cloudTransitionSpeed.get());
			p.y = ofLerp(p.y, target.y, cloudTransitionSpeed.get());
			pointsMoved = true;
		} else {
			p.x = target.x;
			p.y = target.y;
		}
	}

	if (pointsMoved && spatialGrid && !points.empty()) {
		// Rebuild the spatial grid because point positions have changed
		spatialGrid = std::make_shared<SpatialGrid>(points, 50.0f / zoom);
	}

	// Navigate Mode Scrubbing
	if (currentMode == NAVIGATE && spatialGrid) {
	}

	// Browse Mode Logic
	if (currentMode == BROWSE && spatialGrid) {
		// While adjusting radius/speed with keyboard+drag, suppress browse triggering.
		if (bHoldingR || bHoldingV) {
			if (hasLastHoveredPoint) {
				oscManager.stopSample(mediaRoot + lastHoveredPoint.filename, "path-0");
				hasLastHoveredPoint = false;
				hasHoveredPoint = false;
			}
		} else {
			if (ofGetMousePressed()) {
			ofVec2f worldPos = screenToWorld(ofGetMouseX(), ofGetMouseY());
			std::vector<DataPoint> nearest = spatialGrid->findNearestNeighbors(worldPos.x, worldPos.y, 1);

			if (!nearest.empty()) {
				DataPoint nearestPoint = nearest[0];
				float distance = worldPos.distance(ofVec2f(nearestPoint.x, nearestPoint.y));
				float threshold = 20.0f / zoom;

				if (distance > threshold) {
					// Too far
					if (hasLastHoveredPoint) {
						oscManager.stopSample(mediaRoot + lastHoveredPoint.filename, "path-0");
						hasLastHoveredPoint = false;
						hasHoveredPoint = false;
					}
				} else {
					// Close enough
					hoveredPoint = nearestPoint;
					hasHoveredPoint = true;

					// Track selected point index for neighbour mode
					if (neighbourModeActive) {
						for (int pi = 0; pi < (int)points.size(); ++pi) {
							if (points[pi].filename == nearestPoint.filename) {
								selectedPointIdx = pi;
								break;
							}
						}
					}

					// If different from last hovered
					if (!hasLastHoveredPoint || !(hoveredPoint == lastHoveredPoint)) {
						if (hasLastHoveredPoint) {
							oscManager.stopSample(mediaRoot + lastHoveredPoint.filename, "path-0");
						}

						// Trigger new
						float vol = 0.5f;
						string mode = "once";
						if (paths.size() > 0) {
							vol = paths[0]->volume;
							mode = paths[0]->getMode();
						}

						oscManager.sendSample(mediaRoot + hoveredPoint.filename, "path-0", vol, mode);
						noteTriggeredMediaForSpectrogram(mediaRoot + hoveredPoint.filename);
						// possibly trigger video
						if (showVideo) triggerVideo(hoveredPoint);
						lastHoveredPoint = hoveredPoint;
						hasLastHoveredPoint = true;
					}
				}
			}
			} else {
				// Mouse not pressed - stop playing if we were
				if (hasLastHoveredPoint) {
					oscManager.stopSample(mediaRoot + lastHoveredPoint.filename, "path-0");
					hasLastHoveredPoint = false;
					hasHoveredPoint = false;
				}
			}
		}
	}

	// Neighbour mode: tick sequential playback queue
	neighbourSeqGapMs = neighbourSeqGapMs_param.get(); // sync from GUI
	if (neighbourSeqPlaying && !neighbourQueue.empty()) {
		uint64_t nowMs = ofGetElapsedTimeMillis();
		if ((nowMs - neighbourSeqLastTriggerMs) >= (uint64_t)neighbourSeqGapMs) {
			if (neighbourSeqIdx < (int)neighbourQueue.size()) {
				int ptIdx = neighbourQueue[neighbourSeqIdx];
				const DataPoint & np = points[ptIdx];
				float baseVol = paths.empty() ? 0.5f : paths[0]->volume;
				float volScale = 1.0f;
				if (neighbourQueueDistances.size() == neighbourQueue.size() && !neighbourQueueDistances.empty()) {
					float minD = *std::min_element(neighbourQueueDistances.begin(), neighbourQueueDistances.end());
					float maxD = *std::max_element(neighbourQueueDistances.begin(), neighbourQueueDistances.end());
					float range = std::max(maxD - minD, 1e-6f);
					float d = neighbourQueueDistances[neighbourSeqIdx];
					float nearWeight = 1.0f - ((d - minD) / range); // near=1, far=0
					nearWeight = std::clamp(nearWeight, 0.0f, 1.0f);
					volScale = ofLerp(0.2f, 1.0f, nearWeight); // far still audible
				}
				float vol = baseVol * volScale;
				string mode = paths.empty() ? "once" : paths[0]->getMode();
				oscManager.sendSample(mediaRoot + np.filename, "path-0", vol, mode);
				noteTriggeredMediaForSpectrogram(mediaRoot + np.filename);
				// Trigger video if enabled
				if (showVideo) {
					triggerVideo(np);
				}
				// Record for line illumination
				neighbourLastPlayedIdx = ptIdx;
				neighbourLastPlayedMs = nowMs;
				neighbourSeqLastTriggerMs = nowMs;
				++neighbourSeqIdx;
			} else {
				neighbourSeqPlaying = false; // Done
			}
		}
	}

	// Update Paths
	float dt = 1.0f / 60.0f; // Fixed time step for simplicity

	for (auto & path : paths) {
		int prevStepIndex = path->currentStepIndex; // snapshot before update (for stepMode detection)
		path->update(dt);

		float effectiveVolume = path->volume;

		// Audio Sampling Logic
		if (path->isActive && spatialGrid) {
			// Get active points
			std::vector<DataPoint> activePoints = path->getActivePoints(points, *spatialGrid);

			// ---------------------------------------------------------
			// SEQUENTIAL PATH LOGIC
			// ---------------------------------------------------------
			if (path->isSequential) {
				int totalSteps = path->sequentialPoints.size();
				if (totalSteps > 0) {

					// Step mode: PathObject::update() already advanced currentStepIndex via timer.
					// Just detect the change and trigger/stop samples.
					if (path->stepMode) {
						if (path->currentStepIndex != prevStepIndex && path->currentStepIndex >= 0) {
							// Stop previous
							if (prevStepIndex >= 0 && prevStepIndex < totalSteps) {
								DataPoint & prev = path->sequentialPoints[prevStepIndex];
								oscManager.stopSample(mediaRoot + prev.filename, path->name);
								path->playingPoints.erase(prev);
							}
							// Play new
							if (path->currentStepIndex < totalSteps) {
								DataPoint & curr = path->sequentialPoints[path->currentStepIndex];
								oscManager.sendSample(mediaRoot + curr.filename, path->name, effectiveVolume, path->getMode());
								noteTriggeredMediaForSpectrogram(mediaRoot + curr.filename);
								path->playingPoints.insert(curr);
								if (showVideo && path->sendToVideo) triggerVideo(curr);
							}
						}
						continue; // skip posFloor logic
					}
					// Compute the position-derived floor (used for boundary detection in both modes)
					int posFloor = (int)(path->position * totalSteps);
					if (posFloor >= totalSteps) posFloor = totalSteps - 1;

					int nextStepIndex;
					bool stepChanged = false;

					if (path->jitterMode) {
						// Jitter mode: use posFloor only to detect when a boundary is crossed.
						// The actual step is chosen probabilistically from currentStepIndex.
						if (posFloor != path->lastJitterStepFloor) {
							// A boundary was crossed — decide where to go
							path->lastJitterStepFloor = posFloor;
							float r = ofRandom(1.0f);
							if (r < 0.15f) {
								// 15%: go back one
								nextStepIndex = path->currentStepIndex - 1;
							} else if (r < 0.85f) {
								// 70%: repeat (stay)
								nextStepIndex = path->currentStepIndex;
							} else {
								// 15%: advance one
								nextStepIndex = path->currentStepIndex + 1;
							}
							// Wrap circularly
							nextStepIndex = ((nextStepIndex % totalSteps) + totalSteps) % totalSteps;

							// Force trigger for jitter even if the step index didn't change
							stepChanged = true;
						} else {
							// No boundary crossed this frame
							nextStepIndex = path->currentStepIndex;
							stepChanged = false;
						}
					} else {
						// Normal sequential: advance exactly ONE step per frame toward posFloor,
						// but jump directly if posFloor wrapped (big negative jump = loop restart).
						if (path->currentStepIndex < 0) {
							nextStepIndex = posFloor; // first frame
						} else {
							int diff = posFloor - path->currentStepIndex;
							bool wrapped = diff < -(totalSteps / 2);
							if (wrapped) {
								// Position looped — jump directly to start of new cycle
								nextStepIndex = posFloor;
							} else if (diff > 0) {
								nextStepIndex = path->currentStepIndex + 1;
							} else if (diff < 0) {
								nextStepIndex = path->currentStepIndex - 1;
							} else {
								nextStepIndex = path->currentStepIndex;
							}
						}
						stepChanged = (nextStepIndex != path->currentStepIndex);
					}

					// Handle step transition
					if (stepChanged) {
						// Stop previous (only if we are moving to a different index, or if it's the same index allow it to overlap or stop/restart)
						// For granular/loops, stopping and restarting the same sample might clip, but for 'once' it acts as a retrigger.
						if (path->currentStepIndex >= 0 && path->currentStepIndex < totalSteps) {
							DataPoint & prev = path->sequentialPoints[path->currentStepIndex];
							// Don't stop it if it's literally the same sample we are about to re-trigger, unless it's ONCE mode where we might want to restart?
							// Actually, to make "stay" audible as a rhythm, we should stop and restart, or just fire a new instance.
							// ofxOscManager stopSample sends an OSC array release. Let's stop the previous explicitly.
							oscManager.stopSample(mediaRoot + prev.filename, path->name);
							path->playingPoints.erase(prev);
						}
						// Play new (or same)
						if (nextStepIndex >= 0 && nextStepIndex < totalSteps) {
							DataPoint & curr = path->sequentialPoints[nextStepIndex];
							oscManager.sendSample(mediaRoot + curr.filename, path->name, effectiveVolume, path->getMode());
							noteTriggeredMediaForSpectrogram(mediaRoot + curr.filename);
							path->playingPoints.insert(curr);
							if (showVideo && path->sendToVideo) triggerVideo(curr);
						}
						path->currentStepIndex = nextStepIndex;
					} else if (path->currentStepIndex < 0 && posFloor >= 0) {
						// First frame: start playback
						int startIdx = path->jitterMode ? 0 : posFloor;
						if (startIdx < totalSteps) {
							DataPoint & curr = path->sequentialPoints[startIdx];
							oscManager.sendSample(mediaRoot + curr.filename, path->name, effectiveVolume, path->getMode());
							noteTriggeredMediaForSpectrogram(mediaRoot + curr.filename);
							path->playingPoints.insert(curr);
							if (showVideo) triggerVideo(curr);
							path->currentStepIndex = startIdx;
							path->lastJitterStepFloor = posFloor;
						}
					}
				}

				// Skip the spatial radius logic below for sequential paths
				continue;
			}

			if (!path->isActive) continue;
			if (path->name == "path-0") continue; // Skip the default browse path

			// ---------------------------------------------------------
			// NEW LOGIC: Managed Playing Points
			// ---------------------------------------------------------

			// 1. Clean up playing points that have left the radius
			// Create a list of points to remove to avoid invalidating iterator
			std::vector<DataPoint> toRemove;
			for (const auto & p : path->playingPoints) {
				bool stillInRadius = false;
				for (const auto & ap : activePoints) {
					if (ap == p) {
						stillInRadius = true;
						break;
					}
				}
				if (!stillInRadius) {
					toRemove.push_back(p);
				}
			}

			// Remove and Stop
			for (const auto & p : toRemove) {
				oscManager.stopSample(mediaRoot + p.filename, path->name);
				path->playingPoints.erase(p);
			}

			// 2. Add new points if we have slots available
			int slotsAvailable = path->sampleNum - path->playingPoints.size();
			if (slotsAvailable > 0) {
				for (const auto & p : activePoints) {
					if (slotsAvailable <= 0) break;

					// If not already playing
					if (path->playingPoints.find(p) == path->playingPoints.end()) {
						// Add and Start
						path->playingPoints.insert(p);
						slotsAvailable--;

						// Trigger Sound
						if (path->mode == PathObject::LOOP_MODE || path->mode == PathObject::MIXED_MODE || path->mode == PathObject::ONCE_MODE) {
							// Calculate volume based on distance
							float d = path->getDistanceToPoint(ofVec2f(p.x, p.y));
							float vol = effectiveVolume * (1.0f - (d / path->radius));
							vol = MAX(0, vol);
							oscManager.sendSample(mediaRoot + p.filename, path->name, vol, path->getMode());
							noteTriggeredMediaForSpectrogram(mediaRoot + p.filename);
						}

						// Trigger Video
						if (showVideo && path->sendToVideo) triggerVideo(p);
					}
				}
			}

			// 3. Continuous Updates for Playing Points
			if (path->mode == PathObject::LOOP_MODE || path->mode == PathObject::MIXED_MODE || path->mode == PathObject::CLOUD_MODE || path->mode == PathObject::ONCE_MODE) {
				for (const auto & p : path->playingPoints) {
					float d = path->getDistanceToPoint(ofVec2f(p.x, p.y));
					float vol = effectiveVolume * (1.0f - (d / path->radius));
					vol = MAX(0, vol);

					if (path->mode == PathObject::LOOP_MODE || path->mode == PathObject::MIXED_MODE || path->mode == PathObject::ONCE_MODE) {
						// Send volume update (if we had a generic update message, or just re-send sample command with updated vol?
						// OscManager::sendSample triggers a new play. We need a volume update or parameter update.
						// OscManager currently has sendPathVolume (global for path) but not per-voice volume unless we use specific voice allocation API.
						// The previous code re-sent sendSample? No, previous code only sent sendSample on 'isNew'.
						// It didn't update volume continuously for loop mode?
						// Looking at previous code:
						// if (isNew) { ... oscManager.sendSample(...) }
						// Continuous update was ONLY for "cloud" or "mixed".
						// So for "loop", we typically just start it.
						// BUT, if we want distance-based attenuation, we should probably update it.
						// However, sendSample restarts the sample in many simple OSC implementations.
						// Let's stick to previous behavior: Only start it.
						// The only continuous update in previous code was for Granular ("cloud").
					}

					if (path->mode == PathObject::CLOUD_MODE || path->mode == PathObject::MIXED_MODE) {
						// Build smoother, correlated modulation so grain updates evolve instead of pure white jitter.
						float posBase = ofMap(path->position, 0.0f, 1.0f, path->gPosMin, path->gPosMax, true);
						float posJitter = ofRandom(-path->gRand, path->gRand) * 0.35f;
						float pos = ofClamp(posBase + posJitter, path->gPosMin, path->gPosMax);

						path->grainRateWalk = ofClamp(path->grainRateWalk + ofRandom(-0.08f, 0.08f), 0.0f, 1.0f);
						path->grainDurWalk = ofClamp(path->grainDurWalk + ofRandom(-0.06f, 0.06f), 0.0f, 1.0f);
						path->grainDensityWalk = ofClamp(path->grainDensityWalk + ofRandom(-0.09f, 0.09f), 0.0f, 1.0f);

						path->grainRateWalk = ofLerp(path->grainRateWalk, 0.5f, 0.01f);
						path->grainDurWalk = ofLerp(path->grainDurWalk, 0.5f, 0.01f);
						path->grainDensityWalk = ofLerp(path->grainDensityWalk, 0.5f, 0.01f);

						path->grainMotionPhase += dt * ofMap(path->speed, 0.0f, 0.02f, 0.08f, 0.9f, true);
						if (path->grainMotionPhase > TWO_PI) path->grainMotionPhase -= TWO_PI;
						float lfo = 0.5f + 0.5f * sin(path->grainMotionPhase);

						float rateNorm = ofClamp(path->grainRateWalk * 0.75f + lfo * 0.25f, 0.0f, 1.0f);
						float durNorm = ofClamp(path->grainDurWalk * 0.75f + (1.0f - lfo) * 0.25f, 0.0f, 1.0f);
						float densNorm = ofClamp(path->grainDensityWalk * 0.65f + lfo * 0.35f, 0.0f, 1.0f);

						float rate = ofLerp(path->gRateMin, path->gRateMax, rateNorm);
						float dur = ofLerp(path->gDurMin, path->gDurMax, durNorm);
						int grainDensity = (int)std::round(ofLerp((float)path->gGrainRateMin, (float)path->gGrainRateMax, densNorm));
						grainDensity = ofClamp(grainDensity, path->gGrainRateMin, path->gGrainRateMax);

						oscManager.updateGrain(mediaRoot + p.filename, path->name,
							rate,
							pos,
							vol,
							dur,
							grainDensity,
							path->gRand,
							path->gEnv);
					}
				}
			}

			// Update lastActivePoints just in case other things need it, or remove it if unused.
			// The draw loop uses it to draw lines! So we must keep it matching playingPoints or activePoints?
			// Draw loop: "Draw lines to active points ... for (const auto & p : lastActivePoints)"
			// If we want lines to only drawn to PLAYING points, we should update lastActivePoints to be playingPoints.
			path->lastActivePoints = path->playingPoints;
		}
	}

	if (showVideo) {
		if (videoDisplayMode.get() == 0) {
			videoFront->update();
			videoBack->update();

			// Detect loop to hide gap
			float currentPos = videoFront->getPosition();
			if (currentPos < 0.1f && videoLastPos > 0.8f) {
				// We just looped (either naturally or manually)
				videoIgnoreFrames = 4; // hide video for 4 frames to let hold buffer show
			}
			videoLastPos = currentPos;

			// Hack for AVFoundation loop gap: manually restart video before it hits the absolute end
			// This prevents it from outputting a black frame during its native loop cycle
			if (videoFront->isPlaying() && currentPos > 0.98f) {
				videoFront->setPosition(0.0f);
			}
			// Sync fade speed from GUI param
			videoFadeSpeed = videoFadeSpeed_param.get();
			// Advance crossfade alpha toward target
			if (videoAlpha < videoAlphaTarget)
				videoAlpha = std::min(videoAlpha + videoFadeSpeed, videoAlphaTarget);
			else if (videoAlpha > videoAlphaTarget)
				videoAlpha = std::max(videoAlpha - videoFadeSpeed, videoAlphaTarget);

			if (videoIgnoreFrames > 0) {
				videoIgnoreFrames--;
			}

			// Captures the front player's current frame to the hold FBO.
			// Only fires when a genuinely new frame has been decoded AND the clip is fully faded in AND we aren't hiding a loop gap.
			if (videoFront->isFrameNew() && videoFront->getWidth() > 0 && videoAlpha >= 255.0f && videoIgnoreFrames == 0) {
				// Lazily allocate here so window size is definitely known
				if (!videoHoldAllocated) {
					videoHoldFbo.allocate(ofGetWidth(), ofGetHeight(), GL_RGB);
					videoHoldAllocated = true;
				}
				videoHoldFbo.begin();
				ofClear(backgroundColor.get().r,
					backgroundColor.get().g,
					backgroundColor.get().b, 255);
				ofSetColor(255);
				// Draw with current fit mode into the FBO
				float vw = videoFront->getWidth(), vh = videoFront->getHeight();
				float sw = (float)ofGetWidth(), sh = (float)ofGetHeight();
				float x = 0, y = 0, dw = sw, dh = sh;
				int fitMode = videoFitMode.get();
				if (fitMode == 1 && vh > 0) {
					dh = sh;
					dw = (vw / vh) * sh;
					x = (sw - dw) * 0.5f;
					y = 0;
				} else if (fitMode == 2 && vw > 0) {
					dw = sw;
					dh = (vh / vw) * sw;
					x = 0;
					y = (sh - dh) * 0.5f;
				}
				videoFront->draw(x, y, dw, dh);
				videoHoldFbo.end();
			}
		} else if (videoDisplayMode.get() == 2) {
			// GHOST MODE — update all active ghost layers
			for (auto & gp : ghostPlayers) {
				if (gp->isLoaded() && gp->isPlaying()) gp->update();
			}
		} else if (videoDisplayMode.get() == 3) {
			// MAPPED MODE — update all active mapped clips
			uint64_t nowMs = ofGetElapsedTimeMillis();
			bool allowLowFpsUpdate = (nowMs - mappedLowFpsLastUpdateMs) >= 66; // ~15 FPS
			if (allowLowFpsUpdate) mappedLowFpsLastUpdateMs = nowMs;

			float centerX = ofGetWidth() * 0.5f;
			float centerY = ofGetHeight() * 0.5f;
			float margin = 220.0f;
			for (size_t i = 0; i < mappedPlayers.size(); ++i) {
				auto & mc = mappedPlayers[i];
				if (!mc.player->isLoaded() || !mc.player->isPlaying()) continue;

				// Newest clip always updates (highest frame rate).
				bool isTopMost = (i + 1 == mappedPlayers.size());
				if (!isTopMost && !allowLowFpsUpdate) continue;

				float sx = mc.point.x * zoom + pan.x + centerX;
				float sy = mc.point.y * zoom + pan.y + centerY;
				bool onScreen = (sx > -margin && sx < ofGetWidth() + margin && sy > -margin && sy < ofGetHeight() + margin);
				if (isTopMost || onScreen) {
					mc.player->update();
				}
			}
		} else if (videoDisplayMode.get() == 4) {
			// COLLAGE MODE
			if (!videoQueue.empty() && !collagePlayers.empty()) {
				string nextVideo = videoQueue.front();
				videoQueue.pop_front();

				int collageCount = (int)collagePlayers.size();
				int tileIdx = (collageWriteCounter * 7) % collageCount; // 7 is coprime with 20
				collageWriteCounter++;

				auto & player = collagePlayers[tileIdx];
				player->stop();
				player->close();
				player->load(nextVideo);
				player->setLoopState(OF_LOOP_NORMAL);
				player->play();
				player->setVolume(0);

				// Slight per-tile randomization for a photographic collage feel.
				collageAnglesDeg[tileIdx] = ofRandom(-5.0f, 5.0f);
				collageOffsetsPx[tileIdx] = ofVec2f(ofRandom(-10.0f, 10.0f), ofRandom(-10.0f, 10.0f));
			}

			for (auto & player : collagePlayers) {
				if (player->isLoaded() && player->isPlaying()) {
					player->update();
				}
			}
		} else if (videoDisplayMode.get() == 5) {
			// TILE MOSAIC MODE
			if (!videoQueue.empty()) {
				string nextVideo = videoQueue.front();
				videoQueue.pop_front();

				int newSlot;
				if ((int)mosaicHistorySlots.size() < kMosaicMaxHistory) {
					// Use the next fresh slot from the pre-allocated pool
					newSlot = (int)mosaicHistorySlots.size();
				} else {
					// Evict the oldest slot and reuse its player
					int evictedSlot = mosaicHistorySlots.back();
					mosaicHistorySlots.pop_back();
					mosaicPool[evictedSlot]->stop();
					mosaicPool[evictedSlot]->close();
					// Tiles pointing to the evicted slot fall back to the next-oldest
					int fallback = mosaicHistorySlots.empty() ? -1 : mosaicHistorySlots.back();
					for (auto & a : mosaicTileAssignment) {
						if (a == evictedSlot) a = fallback;
					}
					newSlot = evictedSlot;
				}

				mosaicPool[newSlot]->load(nextVideo);
				mosaicPool[newSlot]->setLoopState(OF_LOOP_NORMAL);
				mosaicPool[newSlot]->play();
				mosaicPool[newSlot]->setVolume(0);
				mosaicHistorySlots.push_front(newSlot);

				// Randomly replace a fraction of tiles with the new video.
				// On the very first clip, fill all tiles to avoid empty gaps.
				int totalTiles = kMosaicCols * kMosaicRows;
				int numToReplace = (int)std::round(totalTiles * ofClamp(mosaicReplaceRatio.get(), 0.0f, 1.0f));
				numToReplace = std::max(0, std::min(totalTiles, numToReplace));
				if (mosaicHistorySlots.size() == 1) {
					numToReplace = totalTiles;
				}
				std::vector<int> tileIndices(totalTiles);
				for (int i = 0; i < totalTiles; i++) tileIndices[i] = i;
				// Fisher-Yates shuffle
				for (int i = totalTiles - 1; i > 0; --i) {
					int j = (int)ofRandom(0.0f, (float)(i + 1));
					std::swap(tileIndices[i], tileIndices[j]);
				}
				for (int i = 0; i < numToReplace; i++) {
					mosaicTileAssignment[tileIndices[i]] = newSlot;
				}
			}

			// Update all active players in the history
			for (int slot : mosaicHistorySlots) {
				auto & player = mosaicPool[slot];
				if (player->isLoaded() && player->isPlaying()) {
					player->update();

					// Persist the most recent valid frame to avoid transient black tiles.
					if (player->getWidth() > 0 && player->getHeight() > 0) {
						ofTexture & liveTex = player->getTexture();
						if (liveTex.isAllocated()) {
							int fw = player->getWidth();
							int fh = player->getHeight();
							if (!mosaicHoldAllocated[slot]
								|| mosaicHoldFbos[slot].getWidth() != fw
								|| mosaicHoldFbos[slot].getHeight() != fh) {
								mosaicHoldFbos[slot].allocate(fw, fh, GL_RGB);
								mosaicHoldAllocated[slot] = true;
							}
							if (player->isFrameNew()) {
								mosaicHoldFbos[slot].begin();
								ofSetColor(255);
								liveTex.draw(0, 0, fw, fh);
								mosaicHoldFbos[slot].end();
							}
						}
					}
				}
			}
		} else {
			// GRID MODE

			// 1. Shift videos when a new one is queued
			if (!videoQueue.empty()) {
				string nextVideo = videoQueue.front();
				videoQueue.pop_front();

				// Retrieve the last player in the array (evicted)
				auto evictedPlayer = gridPlayers.back();
				evictedPlayer->stop();
				evictedPlayer->close();

				// Shift all elements one step back
				for (int i = gridPlayers.size() - 1; i > 0; --i) {
					gridPlayers[i] = gridPlayers[i - 1];
				}

				// Place the evicted (now fresh) player at [0]
				gridPlayers[0] = evictedPlayer;

				// Start the new clip in cell [0]
				gridPlayers[0]->load(nextVideo);
				gridPlayers[0]->setLoopState(OF_LOOP_NORMAL);
				gridPlayers[0]->play();
				gridPlayers[0]->setVolume(0);
			}

			// 2. Continuous updates on all active cell players
			for (auto & player : gridPlayers) {
				if (player->isLoaded() && player->isPlaying()) {
					player->update();
				}
			}
		}
	}

	// Mouse Scrubbing Update
	// Mouse Scrubbing Update - REMOVED for Navigate Mode
	if (currentMode == NAVIGATE && spatialGrid && !paths.empty()) {
		// Disabled scrubbing in NAVIGATE mode as per request.
		// Clearing any lingering mouse active points if needed.
		if (!mouseActivePoints.empty()) {
			for (const auto & p : mouseActivePoints) {
				oscManager.stopSample(mediaRoot + p.filename, "mouse");
			}
			mouseActivePoints.clear();
		}
	}

	// Update annotations (tracks anchor points each frame)
	annotationManager.update(points);
}

//--------------------------------------------------------------
void ofApp::drawVisuals() {
	uint64_t nowMsDraw = ofGetElapsedTimeMillis();
	float onsetPulseDraw = 0.0f;
	if (lastOnsetTimeMs > 0 && nowMsDraw >= lastOnsetTimeMs) {
		float dt = (float)(nowMsDraw - lastOnsetTimeMs);
		onsetPulseDraw = ofClamp(1.0f - (dt / (float)ONSET_VISUAL_DURATION_MS), 0.0f, 1.0f);
	}
	float energyNormDraw = ofClamp(audioEnergy, 0.0f, 1.0f);
	if (!audioVisualFeedbackEnabled.get()) {
		onsetPulseDraw = 0.0f;
		energyNormDraw = 0.0f;
	}
	float audioReactivePulseDraw = (energyNormDraw * audioEnergyVisualAmount.get())
		+ (onsetPulseDraw * audioOnsetVisualAmount.get());

	// Draw Video Background
	if (showVideo) {
		if (videoDisplayMode.get() == 0) {
			if (videoHoldAllocated) {
				// Paint the last-captured frame as a persistent background.
				// This is always opaque — covers any gap/flicker from the live player.
				ofBackground(backgroundColor);
				ofSetColor(255, 255, 255, 255);
				videoHoldFbo.draw(0, 0, ofGetWidth(), ofGetHeight());
			} else {
				ofBackground(backgroundColor); // before first frame is ever captured
			}

			// Draw the live front player on top as it fades in (0 → 255 on clip switch).
			// If we are currently ignoring frames (to hide a loop gap), we just skip drawing it,
			// letting the frozen `videoHoldFbo` seamlessly fill the gap!
			if (videoFront->isLoaded() && videoFront->getWidth() > 0 && videoIgnoreFrames == 0) {
				ofEnableBlendMode(OF_BLENDMODE_ALPHA);
				ofSetColor(255, 255, 255, (int)std::clamp(videoAlpha, 0.0f, 255.0f));
				float vw = videoFront->getWidth(), vh = videoFront->getHeight();
				float sw = (float)ofGetWidth(), sh = (float)ofGetHeight();
				float x = 0, y = 0, dw = sw, dh = sh;
				int fitMode = videoFitMode.get();
				if (fitMode == 1 && vh > 0) {
					dh = sh;
					dw = (vw / vh) * sh;
					x = (sw - dw) * 0.5f;
					y = 0;
				} else if (fitMode == 2 && vw > 0) {
					dw = sw;
					dh = (vh / vw) * sw;
					x = 0;
					y = (sh - dh) * 0.5f;
				}
				videoFront->draw(x, y, dw, dh);
				ofDisableBlendMode();
			}
			ofSetColor(255);
		} else if (videoDisplayMode.get() == 1) {
			// GRID MODE
			ofBackground(backgroundColor);

			float sw = (float)ofGetWidth();
			float sh = (float)ofGetHeight();
			float cellWidth = sw / GRID_COLS;
			float cellHeight = sh / GRID_ROWS;
			float gutter = 2.0f; // 2px uniform gutter

			ofSetColor(255);
			for (size_t i = 0; i < gridPlayers.size(); ++i) {
				auto & player = gridPlayers[i];
				if (player->isLoaded() && player->getWidth() > 0) {
					int col = i % GRID_COLS;
					int row = i / GRID_COLS;

					// Compute bounding box for cell with gutter
					float cx = col * cellWidth + gutter;
					float cy = row * cellHeight + gutter;
					float cw = cellWidth - (gutter * 2);
					float ch = cellHeight - (gutter * 2);

					// Render video based on fit mode
					float vw = player->getWidth(), vh = player->getHeight();
					float x = cx, y = cy, dw = cw, dh = ch;
					int fitMode = videoFitMode.get();

					if (fitMode == 1 && vh > 0) {
						dh = ch;
						dw = (vw / vh) * ch;
						x = cx + (cw - dw) * 0.5f;
						y = cy;
					} else if (fitMode == 2 && vw > 0) {
						dw = cw;
						dh = (vh / vw) * cw;
						x = cx;
						y = cy + (ch - dh) * 0.5f;
					}

					player->draw(x, y, dw, dh);
				}
			}
		} else if (videoDisplayMode.get() == 2) {
			// BLEND MIX MODE
			// A controlled collage stack using alternating blend modes to avoid overexposure.
			ofBackground(backgroundColor);

			int n = (int)ghostPlayers.size();
			for (int i = 0; i < n; ++i) {
				auto & gp = ghostPlayers[i];
				if (!gp->isLoaded() || gp->getWidth() <= 0) continue;

				float vw = gp->getWidth(), vh = gp->getHeight();
				float sw = (float)ofGetWidth(), sh = (float)ofGetHeight();
				float x = 0, y = 0, dw = sw, dh = sh;
				int fitMode = videoFitMode.get();
				if (fitMode == 1 && vh > 0) {
					dh = sh;
					dw = (vw / vh) * sh;
					x = (sw - dw) * 0.5f;
					y = 0;
				} else if (fitMode == 2 && vw > 0) {
					dw = sw;
					dh = (vh / vw) * sw;
					x = 0;
					y = (sh - dh) * 0.5f;
				}

				if (i == 0) {
					// Newest layer as anchor image.
					ofEnableBlendMode(OF_BLENDMODE_ALPHA);
					ofSetColor(255, 255, 255, 210);
				} else if (i % 3 == 1) {
					ofEnableBlendMode(OF_BLENDMODE_MULTIPLY);
					ofSetColor(255, 255, 255, 110);
				} else if (i % 3 == 2) {
					ofEnableBlendMode(OF_BLENDMODE_SCREEN);
					ofSetColor(255, 255, 255, 70);
				} else {
					// Difference-like accent (subtractive) on occasional deeper layers.
					ofEnableBlendMode(OF_BLENDMODE_SUBTRACT);
					ofSetColor(255, 255, 255, 45);
				}

				gp->draw(x, y, dw, dh);
			}

			ofDisableBlendMode();
			ofSetColor(255);
		} else if (videoDisplayMode.get() == 3) {
			// DATA-MAPPED MODE
			// Draw background only here; mapped clips are rendered later above point cloud.
			ofBackground(backgroundColor);
			ofSetColor(255);
		} else if (videoDisplayMode.get() == 4) {
			// TILE COLLAGE MODE
			ofBackground(backgroundColor);

			float sw = (float)ofGetWidth();
			float sh = (float)ofGetHeight();
			float cellWidth = sw / kCollageCols;
			float cellHeight = sh / kCollageRows;
			float gutter = 8.0f;

			for (int i = 0; i < (int)collagePlayers.size(); ++i) {
				auto & player = collagePlayers[i];
				if (!player->isLoaded() || player->getWidth() <= 0) continue;

				int col = i % kCollageCols;
				int row = i / kCollageCols;
				float cx = col * cellWidth + gutter;
				float cy = row * cellHeight + gutter;
				float cw = cellWidth - (gutter * 2.0f);
				float ch = cellHeight - (gutter * 2.0f);

				float centerX = cx + (cw * 0.5f) + collageOffsetsPx[i].x;
				float centerY = cy + (ch * 0.5f) + collageOffsetsPx[i].y;

				float vw = player->getWidth(), vh = player->getHeight();
				float x = -cw * 0.5f, y = -ch * 0.5f, dw = cw, dh = ch;
				int fitMode = videoFitMode.get();
				if (fitMode == 1 && vh > 0) {
					dh = ch;
					dw = (vw / vh) * ch;
					x = -dw * 0.5f;
					y = -dh * 0.5f;
				} else if (fitMode == 2 && vw > 0) {
					dw = cw;
					dh = (vh / vw) * cw;
					x = -dw * 0.5f;
					y = -dh * 0.5f;
				}

				ofPushMatrix();
				ofTranslate(centerX, centerY);
				ofRotateDeg(collageAnglesDeg[i]);
				ofSetColor(255);
				player->draw(x, y, dw, dh);
				// Thin matte border improves collage legibility.
				ofNoFill();
				ofSetColor(235, 235, 235, 180);
				ofSetLineWidth(1.0f);
				ofDrawRectangle(-cw * 0.5f, -ch * 0.5f, cw, ch);
				ofFill();
				ofPopMatrix();
			}
			ofSetColor(255);
		} else if (videoDisplayMode.get() == 5) {
			// TILE MOSAIC MODE
			// Each tile independently shows one of up to kMosaicMaxHistory videos.
			ofBackground(0); // black gutter base

			float sw = (float)ofGetWidth();
			float sh = (float)ofGetHeight();
			float cellWidth = sw / kMosaicCols;
			float cellHeight = sh / kMosaicRows;
			float gutter = 1.0f;

			ofSetColor(255);
			for (int i = 0; i < kMosaicCols * kMosaicRows; i++) {
				int slot = mosaicTileAssignment[i];
				if (slot < 0 || slot >= kMosaicMaxHistory) continue;
				auto & player = mosaicPool[slot];
				if (!player->isLoaded() || player->getWidth() <= 0 || player->getHeight() <= 0) continue;

				int col = i % kMosaicCols;
				int row = i / kMosaicCols;
				float cx = col * cellWidth + gutter;
				float cy = row * cellHeight + gutter;
				float cw = std::max(0.0f, cellWidth - (2.0f * gutter));
				float ch = std::max(0.0f, cellHeight - (2.0f * gutter));

				float vw = player->getWidth();
				float vh = player->getHeight();
				float srcX = ((float)col / (float)kMosaicCols) * vw;
				float srcY = ((float)row / (float)kMosaicRows) * vh;
				float srcW = vw / (float)kMosaicCols;
				float srcH = vh / (float)kMosaicRows;

				// Compose a full-frame image from independently assigned tile sources.
				if (mosaicHoldAllocated[slot]) {
					ofTexture & holdTex = mosaicHoldFbos[slot].getTexture();
					if (holdTex.isAllocated()) {
						holdTex.drawSubsection(cx, cy, cw, ch, srcX, srcY, srcW, srcH);
						continue;
					}
				}

				ofTexture & liveTex = player->getTexture();
				if (liveTex.isAllocated()) {
					liveTex.drawSubsection(cx, cy, cw, ch, srcX, srcY, srcW, srcH);
				}
			}
		}
	} else {
		ofBackground(backgroundColor);
	}

	spectrogramLayerAlpha = std::clamp((float)spectrogramLayerAlpha_param.get(), 0.0f, 1.0f);
	spectrogramTrailAlpha = std::clamp((float)spectrogramTrailAlpha_param.get(), 0.0f, 1.0f);
	spectrogramTrailLength = std::max(1, (int)spectrogramTrailLength_param.get());
	spectrogramLumaKeyThreshold = std::clamp((float)spectrogramLumaKeyThreshold_param.get(), 0.0f, 0.95f);
	spectrogramBlendMode = (SpectrogramBlendMode)std::clamp((int)spectrogramBlendMode_param.get(), 0, 4);
	while ((int)spectrogramTrailImagePaths.size() > spectrogramTrailLength) {
		spectrogramTrailImagePaths.pop_front();
	}

	if (spectrogramLayerEnabled && !spectrogramTrailImagePaths.empty()) {
		ofEnableBlendMode(ofBlendModeFromSpectrogramBlendMode(spectrogramBlendMode));
		int count = std::min((int)spectrogramTrailImagePaths.size(), spectrogramTrailLength);
		int start = std::max(0, (int)spectrogramTrailImagePaths.size() - count);
		for (int i = 0; i < count; ++i) {
			int idx = start + i;
			const std::string & imgPath = spectrogramTrailImagePaths[idx];
			auto specImg = getSpectrogramDisplayCached(imgPath);
			if (!specImg || !specImg->isAllocated() || specImg->getWidth() <= 0 || specImg->getHeight() <= 0) {
				continue;
			}

			float layerAlpha = spectrogramLayerAlpha;
			if (i < count - 1) {
				float rel = (float)(i + 1) / (float)count;
				layerAlpha = spectrogramTrailAlpha * rel;
			}
			int a = (int)std::clamp(layerAlpha * 255.0f, 0.0f, 255.0f);
			ofSetColor(255, 255, 255, a);
			specImg->draw(0, 0, ofGetWidth(), ofGetHeight());
		}
		ofDisableBlendMode();
		ofSetColor(255);
	}

	ofEnableAlphaBlending();

	// Draw grid above video and below points. We project world-grid lines into
	// screen space so changing spacing is immediately visible.
	{
		float worldSpacing = std::max(1e-6f, (float)gridSpacing.get());
		ofColor gc = gridColor.get();
		if (gc.a > 0 && !points.empty()) {
			ofVec2f tl = screenToWorld(0, 0);
			ofVec2f tr = screenToWorld(ofGetWidth(), 0);
			ofVec2f bl = screenToWorld(0, ofGetHeight());
			ofVec2f br = screenToWorld(ofGetWidth(), ofGetHeight());

			float minWX = std::min(std::min(tl.x, tr.x), std::min(bl.x, br.x));
			float maxWX = std::max(std::max(tl.x, tr.x), std::max(bl.x, br.x));
			float minWY = std::min(std::min(tl.y, tr.y), std::min(bl.y, br.y));
			float maxWY = std::max(std::max(tl.y, tr.y), std::max(bl.y, br.y));

			float startWX = std::floor(minWX / worldSpacing) * worldSpacing;
			float endWX = std::ceil(maxWX / worldSpacing) * worldSpacing;
			float startWY = std::floor(minWY / worldSpacing) * worldSpacing;
			float endWY = std::ceil(maxWY / worldSpacing) * worldSpacing;

			ofSetColor(gc);
			ofSetLineWidth(1.0f);
			for (float wx = startWX; wx <= endWX; wx += worldSpacing) {
				float sx = wx * zoom + pan.x + ofGetWidth() * 0.5f;
				ofDrawLine(sx, 0.0f, sx, (float)ofGetHeight());
			}
			for (float wy = startWY; wy <= endWY; wy += worldSpacing) {
				float sy = wy * zoom + pan.y + ofGetHeight() * 0.5f;
				ofDrawLine(0.0f, sy, (float)ofGetWidth(), sy);
			}
		}
	}

	ofPushMatrix();
	ofTranslate(ofGetWidth() / 2, ofGetHeight() / 2);
	ofTranslate(pan);
	ofScale(zoom, zoom);
	float dataAlphaScale = std::clamp(dataVisualAlpha, 0.0f, 1.0f);

	// Build active point lookup from currently playing path points.
	std::unordered_set<std::string> activePointFiles;
	for (const auto & path : paths) {
		for (const auto & p : path->playingPoints) {
			activePointFiles.insert(p.filename);
		}
	}
	for (const auto & p : mouseActivePoints) {
		activePointFiles.insert(p.filename);
	}

	// Point size mapping in world units based on current point-cloud extents.
	// Required mapping: slider 1 => diag/5000, slider 100 => diag.
	float extMinX = std::numeric_limits<float>::max();
	float extMaxX = std::numeric_limits<float>::lowest();
	float extMinY = std::numeric_limits<float>::max();
	float extMaxY = std::numeric_limits<float>::lowest();
	for (const auto & p : points) {
		extMinX = std::min(extMinX, p.x);
		extMaxX = std::max(extMaxX, p.x);
		extMinY = std::min(extMinY, p.y);
		extMaxY = std::max(extMaxY, p.y);
	}
	float pointsDiag = points.empty() ? 1.0f : ofVec2f(extMaxX - extMinX, extMaxY - extMinY).length();
	pointsDiag = std::max(pointsDiag, 1e-6f);
	float sizeT = std::clamp((pointSize.get() - 1.0f) / 99.0f, 0.0f, 1.0f);
	float diagFactor = (1.0f / 1000.0f) * std::pow(1000.0f, sizeT); // log interpolation
	float basePointRadiusWorld = pointsDiag * diagFactor;
	pointGlyphMode = (PointGlyphMode)std::clamp((int)pointGlyphMode_param.get(), 0, 7);
	int thumbnailLoadsRemaining = thumbnailLoadsPerFrame;
	float screenCenterX = ofGetWidth() * 0.5f;
	float screenCenterY = ofGetHeight() * 0.5f;
	float audioScaleBoost = 1.0f;
	audioScaleBoost += audioReactivePulseDraw;

	// Cluster labels for glyph mode 4:
	// unclustered => 1, then clustered groups => 2,3,4... by ascending cluster_id.
	std::unordered_set<int> seenClusterIds;
	for (const auto & p : points) {
		if (p.cluster_id >= 0) seenClusterIds.insert(p.cluster_id);
	}
	std::vector<int> sortedGlyphClusterIds(seenClusterIds.begin(), seenClusterIds.end());
	std::sort(sortedGlyphClusterIds.begin(), sortedGlyphClusterIds.end());
	std::unordered_map<int, int> clusterGlyphLabel;
	for (int ci = 0; ci < (int)sortedGlyphClusterIds.size(); ++ci) {
		clusterGlyphLabel[sortedGlyphClusterIds[ci]] = ci + 2;
	}

	// Draw Points
	ofFill(); // Ensure fill is enabled
	ofColor c = pointColor.get();
	float baseAlpha;
	if (showText) {
		baseAlpha = std::min((float)c.a, 25.0f); // cap opacity so text is readable
	} else {
		baseAlpha = (float)c.a;
	}

	for (int i = 0; i < (int)points.size(); ++i) {
		const auto & p = points[i];
		// Cluster foregrounding: dim non-active cluster points
		float alpha = baseAlpha;
		if (activeClusterId != -999 && p.cluster_id != activeClusterId) {
			alpha = baseAlpha * 0.15f; // 15% of normal
		}

		bool isHovered = hasHoveredPoint && (p.filename == hoveredPoint.filename);
		bool isSelectedPoint = (selectedPointIdx == i) || (hasLastHoveredPoint && (p.filename == lastHoveredPoint.filename));
		bool isAutoActive = (activePointFiles.find(p.filename) != activePointFiles.end());

		ofColor drawColor = c;
		float sizeMultiplier = 1.0f;
		if (isAutoActive) {
			drawColor = activePointColor.get();
			sizeMultiplier = std::max(1.0f, selectedPointSize.get() / std::max(1.0f, pointSize.get()));
		}
		if (isSelectedPoint) {
			drawColor = selectedColor.get();
			sizeMultiplier = std::max(1.0f, selectedPointSize.get() / std::max(1.0f, pointSize.get()));
		}
		if (isHovered) {
			drawColor = hoveredColor.get();
			sizeMultiplier = std::max(1.0f, hoveredPointSize.get() / std::max(1.0f, pointSize.get()));
		}

		ofSetColor(drawColor.r, drawColor.g, drawColor.b, (int)(alpha * dataAlphaScale));

		float scale = 1.0f;
		if (currentThirdDimMode != ThirdDimMode::NONE) {
			float val = 0.0f;
			switch (currentThirdDimMode) {
			case ThirdDimMode::INSTABILITY:
				val = p.instability;
				break;
			case ThirdDimMode::ATTACK:
				val = p.attack;
				break;
			case ThirdDimMode::BRIGHTNESS:
				val = p.brightness;
				break;
			default:
				break;
			}
			scale = ofLerp(0.2f, 2.0f, val);
		}
		float glyphRadiusWorld = basePointRadiusWorld * scale * sizeMultiplier * audioScaleBoost;
		PointGlyphMode drawMode = pointGlyphMode;
		if (drawMode == PointGlyphMode::MIXED) {
			uint32_t mixHash = stableStringHash(p.filename + "|" + p.text + "|" + ofToString(i));
			drawMode = (PointGlyphMode)(mixHash % 6); // avoid thumbnail churn in mixed mode
		}

		switch (drawMode) {
		case PointGlyphMode::CIRCLE:
			ofDrawCircle(p.x, p.y, glyphRadiusWorld);
			break;

		case PointGlyphMode::SQUARE:
			ofDrawRectangle(p.x - glyphRadiusWorld, p.y - glyphRadiusWorld,
				glyphRadiusWorld * 2.0f, glyphRadiusWorld * 2.0f);
			break;

		case PointGlyphMode::X_MARK:
			ofSetLineWidth(1.5f);
			ofDrawLine(p.x - glyphRadiusWorld, p.y - glyphRadiusWorld,
				p.x + glyphRadiusWorld, p.y + glyphRadiusWorld);
			ofDrawLine(p.x - glyphRadiusWorld, p.y + glyphRadiusWorld,
				p.x + glyphRadiusWorld, p.y - glyphRadiusWorld);
			ofSetLineWidth(1.0f);
			break;

		case PointGlyphMode::NUMBER: {
			uint32_t h = stableStringHash(p.filename + "|" + ofToString(i));
			std::string label(1, (char)('0' + (h % 10)));
			if (annotationFont.isLoaded()) {
				ofRectangle b = annotationFont.getStringBoundingBox(label, 0.0f, 0.0f);
				float targetH = std::max(glyphRadiusWorld * 2.1f, 1e-6f);
				float scaleY = targetH / std::max(b.height, 1.0f);
				ofPushMatrix();
				ofTranslate(p.x, p.y);
				ofScale(scaleY, scaleY);
				annotationFont.drawString(label, -b.width * 0.5f, b.height * 0.5f);
				ofPopMatrix();
			} else {
				ofDrawCircle(p.x, p.y, glyphRadiusWorld);
			}
		} break;

		case PointGlyphMode::CLUSTER_NUMBER: {
			int clusterNumber = 1;
			auto it = clusterGlyphLabel.find(p.cluster_id);
			if (it != clusterGlyphLabel.end()) clusterNumber = it->second;
			std::string label = ofToString(clusterNumber);
			if (annotationFont.isLoaded()) {
				ofRectangle b = annotationFont.getStringBoundingBox(label, 0.0f, 0.0f);
				float targetH = std::max(glyphRadiusWorld * 2.1f, 1e-6f);
				float scaleY = targetH / std::max(b.height, 1.0f);
				ofPushMatrix();
				ofTranslate(p.x, p.y);
				ofScale(scaleY, scaleY);
				annotationFont.drawString(label, -b.width * 0.5f, b.height * 0.5f);
				ofPopMatrix();
			} else {
				ofDrawCircle(p.x, p.y, glyphRadiusWorld);
			}
		} break;

		case PointGlyphMode::EMOJI: {
			uint32_t h = stableStringHash(p.filename + "|" + p.text + "|emoji");
			int emojiIdx = (int)(h % kEmojiGlyphs.size());
			const std::string & label = emojiGlyphLikelySupported
				? kEmojiGlyphs[emojiIdx]
				: kEmojiFallbackGlyphs[emojiIdx];
			if (annotationFont.isLoaded()) {
				ofRectangle b = annotationFont.getStringBoundingBox(label, 0.0f, 0.0f);
				float targetH = std::max(glyphRadiusWorld * 2.1f, 1e-6f);
				float scaleY = targetH / std::max(b.height, 1.0f);
				ofPushMatrix();
				ofTranslate(p.x, p.y);
				ofScale(scaleY, scaleY);
				annotationFont.drawString(label, -b.width * 0.5f, b.height * 0.5f);
				ofPopMatrix();
			} else {
				ofDrawCircle(p.x, p.y, glyphRadiusWorld);
			}
		} break;

		case PointGlyphMode::THUMBNAIL: {
			float sx = p.x * zoom + pan.x + screenCenterX;
			float sy = p.y * zoom + pan.y + screenCenterY;
			float screenPad = std::max(32.0f, glyphRadiusWorld * zoom * 4.0f);
			if (sx < -screenPad || sx > ofGetWidth() + screenPad || sy < -screenPad || sy > ofGetHeight() + screenPad) {
				break;
			}

			std::string thumbPath = resolveThumbnailPathForPoint(p);
			auto thumb = getThumbnailCached(thumbPath, thumbnailLoadsRemaining);
			if (thumb && thumb->isAllocated() && thumb->getWidth() > 0 && thumb->getHeight() > 0) {
				float targetH = glyphRadiusWorld * 2.4f;
				float targetW = targetH * ((float)thumb->getWidth() / (float)thumb->getHeight());
				targetW = std::clamp(targetW, glyphRadiusWorld * 1.4f, glyphRadiusWorld * 3.6f);
				ofSetColor(255, 255, 255, (int)(alpha * dataAlphaScale));
				thumb->draw(p.x - targetW * 0.5f, p.y - targetH * 0.5f, targetW, targetH);
			} else {
				ofNoFill();
				ofSetLineWidth(1.0f);
				ofDrawRectangle(p.x - glyphRadiusWorld, p.y - glyphRadiusWorld,
					glyphRadiusWorld * 2.0f, glyphRadiusWorld * 2.0f);
				ofDrawLine(p.x - glyphRadiusWorld, p.y - glyphRadiusWorld,
					p.x + glyphRadiusWorld, p.y + glyphRadiusWorld);
				ofFill();
			}
		} break;

		case PointGlyphMode::MIXED:
			// handled above
			break;
		}
	}

	// --- Neighbour Mode Highlighting ---
	if (neighbourModeActive && selectedPointIdx >= 0 && selectedPointIdx < (int)points.size()) {
		const DataPoint & sel = points[selectedPointIdx];

		// Compute neighbour weights once: weight 1.0 = nearest, 0.0 = furthest
		const auto & neighbours = sel.true_neighbors;
		const auto & distances = sel.true_distances;
		int numNeighbours = (int)std::min(neighbours.size(), distances.size());

		if (numNeighbours > 0) {
			float minDist = *std::min_element(distances.begin(), distances.begin() + numNeighbours);
			float maxDist = *std::max_element(distances.begin(), distances.begin() + numNeighbours);
			float distRange = std::max(maxDist - minDist, 1e-6f);

			uint64_t nowMs = ofGetElapsedTimeMillis();
			uint64_t msSincePlayed = (neighbourLastPlayedIdx >= 0)
				? (nowMs - neighbourLastPlayedMs) : kNeighbourFlashMs + 1;

			ofNoFill();
			ofSetLineWidth(1.5f);

			for (int ni = 0; ni < numNeighbours; ++ni) {
				int idx = neighbours[ni];
				if (idx < 0 || idx >= (int)points.size()) continue;
				const DataPoint & np = points[idx];

				// Proximity weight: nearest=1.0, furthest=0.0
				float weight = 1.0f - ((distances[ni] - minDist) / distRange);
				weight = std::max(0.0f, std::min(1.0f, weight));

				// Base alpha: 20% (dim) to 100% (nearest)
				float baseAlpha = ofLerp(50.0f, 255.0f, weight);

				// Flash: if this point was the last one played, overlay a bright flash
				if (idx == neighbourLastPlayedIdx && msSincePlayed < kNeighbourFlashMs) {
					float flashT = 1.0f - ((float)msSincePlayed / (float)kNeighbourFlashMs);
					// Blend line from flash-white toward the distance-tinted colour
					int flashAlpha = (int)(ofLerp(baseAlpha, 255.0f, flashT) * dataAlphaScale);
					ofSetColor(255, 255, 255, flashAlpha); // bright white flash
				} else {
					// Normal: cool blue-white, opacity = proximity
					ofSetColor(180, 210, 255, (int)(baseAlpha * dataAlphaScale));
				}

				ofDrawLine(sel.x, sel.y, np.x, np.y);
			}

			ofFill();
			ofSetLineWidth(1.0f);
		}

		// Draw selected point indicator: bright ring on top
		ofSetColor(255, 255, 100, (int)(230 * dataAlphaScale)); // bright yellow
		ofNoFill();
		ofSetLineWidth(2.0f);
		ofDrawCircle(sel.x, sel.y, basePointRadiusWorld * 2.5f);
		ofFill();
		ofSetLineWidth(1.0f);
	}

	// Draw Text
	// Draw Paths
	ofColor fadedPlayheadColor = playheadColor.get();
	fadedPlayheadColor.a = (int)(fadedPlayheadColor.a * dataAlphaScale);
	ofColor fadedPathColor = pathColor.get();
	fadedPathColor.a = (int)(fadedPathColor.a * dataAlphaScale);
	ofColor fadedSelectedPathColor = selectedPathColor.get();
	fadedSelectedPathColor.a = (int)(fadedSelectedPathColor.a * dataAlphaScale);
	float wobbleAmount = 0.0f;
	if (audioVisualFeedbackEnabled.get()) {
		wobbleAmount = ofClamp(audioReactivePulseDraw * audioPathWobbleAmount.get(), 0.0f, 1.0f);
	}
	float wobbleWorld = (9.0f / std::max(zoom, 0.001f)) * wobbleAmount;
	float wobbleDeg = std::sin(ofGetElapsedTimef() * 2.1f) * (2.5f * wobbleAmount);
	float wobbleX = std::sin(ofGetElapsedTimef() * 3.7f + 0.6f) * wobbleWorld;
	float wobbleY = std::cos(ofGetElapsedTimef() * 3.1f + 1.2f) * wobbleWorld;

	ofPushMatrix();
	ofTranslate(wobbleX, wobbleY);
	ofRotateDeg(wobbleDeg);
	for (auto & path : paths) {
		path->draw(playheadSize / zoom, fadedPlayheadColor, zoom, pathThickness.get(), selectedPathThickness.get(), fadedPathColor, fadedSelectedPathColor);
	}

	// Draw current path
	if (isDrawingPath && currentPath) {
		ofColor currentPathColor = pathColor.get();
		currentPathColor.a = (int)(currentPathColor.a * dataAlphaScale);
		ofSetColor(currentPathColor);
		ofSetLineWidth(selectedPathThickness.get());
		// Only draw the line, not the playhead circle
		currentPath->polyline.draw();
	}
	ofPopMatrix();

	ofPopMatrix();

	// In mapped mode, draw videos above the point cloud.
	if (showVideo && videoDisplayMode.get() == 3) {
		ofSetColor(255);
		float worldThumbH = (ofGetHeight() * 0.15f) / std::max(zoom, 0.001f);
		float centerX = ofGetWidth() * 0.5f;
		float centerY = ofGetHeight() * 0.5f;
		for (auto & mc : mappedPlayers) {
			if (!mc.player->isLoaded() || mc.player->getWidth() <= 0) continue;
			float sx = mc.point.x * zoom + pan.x + centerX;
			float sy = mc.point.y * zoom + pan.y + centerY;
			float vw = mc.player->getWidth(), vh = mc.player->getHeight();
			float aspect = (vh > 0) ? vw / vh : 1.0f;
			float screenH = worldThumbH * zoom;
			float screenW = screenH * aspect;
			mc.player->draw(sx - screenW * 0.5f, sy - screenH * 0.5f, screenW, screenH);
		}

	}

	// Update annotation font size if changed.
	static float lastAnnotationFontSize = annotationFontSize;
	if (std::abs(lastAnnotationFontSize - annotationFontSize.get()) > 0.5f) {
		bool result = annotationFont.load("lmroman10-italic.otf", annotationFontSize);
		if (!result) annotationFont.load(OF_TTF_SANS, annotationFontSize);
		lastAnnotationFontSize = annotationFontSize;
	}

	// Draw annotations in screen space (above the point cloud)
	annotationManager.draw(points, activeClusterId);

	// ---- Screen-space overlays for selected path ----
	if (selectedPath) {
		float sw = (float)ofGetWidth();
		float sh = (float)ofGetHeight();

		// Crosshair: X = position (0-1), Y = volume (0=bottom, 1=top)
		// When ALT is held, use mouse position directly to avoid one-frame lag
		float cx, cy;
		if (ofGetKeyPressed(OF_KEY_ALT)) {
			cx = (float)ofGetMouseX();
			cy = (float)ofGetMouseY();
		} else {
			cx = selectedPath->position * sw;
			// Use gesture volume when a gesture is active, otherwise static volume
			float displayVol = (selectedPath->hasGesture && !selectedPath->gesturePoints.empty())
				? selectedPath->getGestureVolume(selectedPath->position)
				: selectedPath->volume;
			cy = (1.0f - displayVol) * sh;
		}
		float crossSize = 10.0f;
		ofColor phc = playheadColor.get();
		ofSetColor(phc.r, phc.g, phc.b, (int)(128 * dataAlphaScale)); // 50% opacity
		ofSetLineWidth(2);
		ofDrawLine(cx - crossSize, cy, cx + crossSize, cy);
		ofDrawLine(cx, cy - crossSize, cx, cy + crossSize);
		ofSetLineWidth(1);

		// Gesture overlay (screen-space): draw recorded gesture as a visible curve
		if (selectedPath->hasGesture && !selectedPath->gesturePoints.empty()) {
			ofSetLineWidth(3);
			for (size_t i = 0; i + 1 < selectedPath->gesturePoints.size(); ++i) {
				const auto & a = selectedPath->gesturePoints[i];
				const auto & b = selectedPath->gesturePoints[i + 1];
				float x0 = a.position * sw;
				float y0 = (1.0f - a.volume) * sh;
				float x1 = b.position * sw;
				float y1 = (1.0f - b.volume) * sh;
				int alpha = (int)(((a.volume * 135) + 120) * dataAlphaScale);
				ofSetColor(100, 255, 150, alpha);
				ofDrawLine(x0, y0, x1, y1);
			}
			ofSetLineWidth(1);
		}
	}

	// Draw Text (Screen Space)
	if (showText) {
		ofColor fadedTextColor = textColor.get();
		fadedTextColor.a = (int)(fadedTextColor.a * dataAlphaScale);
		ofSetColor(fadedTextColor);
		if (font.isLoaded()) {
			// Update font size if changed
			static float lastFontSize = fontSize;
			if (abs(lastFontSize - fontSize) > 0.5f) {
				bool result = font.load("lmroman10-regular.otf", fontSize);
				if (!result) font.load(OF_TTF_SANS, fontSize);
				lastFontSize = fontSize;
			}

			ofVec2f center(ofGetWidth() / 2, ofGetHeight() / 2);
			for (const auto & p : points) {
				if (!p.text.empty()) {
					// Calculate screen position: (world * zoom) + pan + center
					ofVec2f screenPos = (ofVec2f(p.x, p.y) * zoom) + pan + center;

					// Simple culling
					if (screenPos.x > -100 && screenPos.x < ofGetWidth() + 100 && screenPos.y > -100 && screenPos.y < ofGetHeight() + 100) {

						ofRectangle bounds = font.getStringBoundingBox(p.text, 0, 0);
						// Draw with drop shadow for readability? optional.
						font.drawString(p.text, screenPos.x - bounds.width / 2, screenPos.y - bounds.height / 2 + bounds.height);
					}
				}
			}
		} else {
			// Fallback Bitmap String
			ofVec2f center(ofGetWidth() / 2, ofGetHeight() / 2);
			for (const auto & p : points) {
				if (!p.text.empty()) {
					ofVec2f screenPos = (ofVec2f(p.x, p.y) * zoom) + pan + center;
					if (screenPos.x > -100 && screenPos.x < ofGetWidth() + 100 && screenPos.y > -100 && screenPos.y < ofGetHeight() + 100) {
						float strWidth = p.text.length() * 8.0f;
						ofDrawBitmapString(p.text, screenPos.x - strWidth / 2, screenPos.y + 4);
					}
				}
			}
		}
	}

	// Draw Title (Screen Space)
	if (showTitle && !compositionTitle.empty()) {
		ofSetColor(titleColor);
		if (titleFont.isLoaded()) {
			// Update font size if changed
			static float lastTitleFontSize = titleFontSize;
			if (abs(lastTitleFontSize - titleFontSize) > 0.5f) {
				bool result = titleFont.load("lmroman10-bold.otf", titleFontSize);
				if (!result) titleFont.load(OF_TTF_SANS, titleFontSize);
				lastTitleFontSize = titleFontSize;
			}

			ofRectangle bounds = titleFont.getStringBoundingBox(compositionTitle, 0, 0);
			float x = (ofGetWidth() - bounds.width) / 2.0f;
			float y = (ofGetHeight() - bounds.height) / 2.0f + bounds.height;

			// Optional drop shadow
			ofSetColor(0, 0, 0, 150);
			titleFont.drawString(compositionTitle, x + 2, y + 2);
			ofSetColor(titleColor);

			titleFont.drawString(compositionTitle, x, y);
		} else {
			float strWidth = compositionTitle.length() * 8.0f;
			float x = (ofGetWidth() - strWidth) / 2.0f;
			float y = ofGetHeight() / 2.0f;

			ofSetColor(0, 0, 0, 150);
			ofDrawBitmapString(compositionTitle, x + 2, y + 2);
			ofSetColor(titleColor);

			ofDrawBitmapString(compositionTitle, x, y);
		}
	}

	// Draw Mouse Scrubbing Feedback
	if (currentMode == NAVIGATE && !mouseActivePoints.empty()) {
		ofPushMatrix();
		ofTranslate(ofGetWidth() / 2, ofGetHeight() / 2);
		ofTranslate(pan);
		ofScale(zoom, zoom);

		ofSetColor(255, 200);
		ofNoFill();
		ofVec2f mousePos(ofGetMouseX(), ofGetMouseY());
		ofVec2f worldMouse = screenToWorld(mousePos.x, mousePos.y);
		ofDrawCircle(worldMouse.x, worldMouse.y, 50.0f / zoom); // Draw scrubbing radius

		for (const auto & p : mouseActivePoints) {
			ofDrawLine(worldMouse.x, worldMouse.y, p.x, p.y);
			ofDrawCircle(p.x, p.y, 4.0f / zoom);
		}
		ofPopMatrix();
	}

	// Draw Marquee
	if (currentMode == NAVIGATE && isMarqueeZooming) {
		ofSetColor(255, 100);
		ofNoFill();
		ofDrawRectangle(marqueeStart.x, marqueeStart.y, marqueeEnd.x - marqueeStart.x, marqueeEnd.y - marqueeStart.y);
		ofSetColor(255, 50);
		ofFill();
		ofDrawRectangle(marqueeStart.x, marqueeStart.y, marqueeEnd.x - marqueeStart.x, marqueeEnd.y - marqueeStart.y);
	}

	// Draw UI
	ofSetColor(debugTextColor.get());
	string modeStr = "";
	switch (currentMode) {
	case NAVIGATE:
		modeStr = "NAVIGATE";
		break;
	case DRAW_FREEHAND:
		modeStr = "DRAW_FREEHAND";
		break;
	case DRAW_LINE:
		modeStr = "DRAW_LINE";
		break;
	case EDIT:
		modeStr = "EDIT";
		break;
	case WANDER:
		modeStr = "WANDER";
		break;
	case BROWSE:
		modeStr = "BROWSE";
		break;
	}
	ofDrawBitmapString("Mode: " + modeStr, 20, 20);
	ofDrawBitmapString("Zoom: " + ofToString(zoom), 20, 40);
	ofDrawBitmapString("Paths: " + ofToString(paths.size()), 20, 60);
	string dimStr = "NONE";
	switch (currentThirdDimMode) {
	case ThirdDimMode::INSTABILITY:
		dimStr = "INSTABILITY";
		break;
	case ThirdDimMode::ATTACK:
		dimStr = "ATTACK";
		break;
	case ThirdDimMode::BRIGHTNESS:
		dimStr = "SPECTRAL CENTROID";
		break;
	default:
		break;
	}
	ofDrawBitmapString("3D Dim: " + dimStr, 20, 80);
	ofDrawBitmapString("Audio Mode: " + string(defaultPathMode == PathObject::LOOP_MODE ? "LOOP" : "ONCE"), 20, 100);
	ofDrawBitmapString("Annotation Mode (Tab): " + string(annotationManager.isEnabled() ? "ON" : "OFF"), 20, 120);
	ofDrawBitmapString("Point Glyph (5 cycle): " + pointGlyphModeName(pointGlyphMode), 20, 140);
	ofDrawBitmapString("Spectrogram Layer (6): " + string(spectrogramLayerEnabled ? "ON" : "OFF"), 20, 160);
	ofDrawBitmapString("Generate Missing Media (7): press to run", 20, 180);
	ofDrawBitmapString("Spectrogram Blend (8): " + spectrogramBlendModeName(spectrogramBlendMode), 20, 200);
	ofDrawBitmapString("Spectrogram Trail (GUI): N=" + ofToString(spectrogramTrailLength)
		+ " layerA=" + ofToString(spectrogramLayerAlpha, 2)
		+ " trailA=" + ofToString(spectrogramTrailAlpha, 2)
		+ " luma=" + ofToString(spectrogramLumaKeyThreshold, 2), 20, 220);
	ofDrawBitmapString("Audio Visual Feedback (9): " + string(audioVisualFeedbackEnabled.get() ? "ON" : "OFF")
		+ " | Energy=" + ofToString(audioEnergy, 4)
		+ " | Wobble=" + ofToString(audioPathWobbleAmount.get(), 2), 20, 240);
	{
		string nbStr = neighbourModeActive ? "ON" : "OFF";
		if (neighbourModeActive && selectedPointIdx >= 0 && selectedPointIdx < (int)points.size()) {
			nbStr += " | " + ofFilePath::getFileName(points[selectedPointIdx].filename);
		}
		ofDrawBitmapString("Neighbour Mode (N): " + nbStr, 20, 260);
	}

	int textY = 280;
	if (selectedPath) {
		ofDrawBitmapString("Selected: " + selectedPath->name + " - Video: " + (selectedPath->sendToVideo ? "ON" : "OFF"), 20, textY);
		textY += 20;
	}

	ofDrawBitmapString("Video Mode (m): " + ofToString(showVideo ? "ON" : "OFF"), 20, textY);
	textY += 20;
	ofDrawBitmapString("Video Trigger (;): " + string(videoTriggerLocked ? "LOCKED" : "UNLOCKED"), 20, textY);
	textY += 20;
	if (showVideo) {
		ofDrawBitmapString("Media Root: " + mediaRoot, 20, textY);
		textY += 20;
		ofDrawBitmapString("Loaded Video: " + ofToString(videoFront->getMoviePath()), 20, textY);
		textY += 20;
		ofDrawBitmapString("Last Attempted: " + lastAttemptedVideoPath, 20, textY);
		textY += 20;
		ofDrawBitmapString("Video Playing: " + ofToString(videoFront->isPlaying()), 20, textY);
		textY += 20;
	}

	if (!mediaGenerationStatusText.empty() && ofGetElapsedTimeMillis() <= mediaGenerationStatusUntilMs) {
		ofSetColor(0, 0, 0, 180);
		ofDrawRectangle(16, ofGetHeight() - 76, std::min(900, ofGetWidth() - 32), 44);
		ofSetColor(255, 255, 255, 255);
		ofDrawBitmapString(mediaGenerationStatusText, 28, ofGetHeight() - 49);
		ofSetColor(debugTextColor.get());
	}

	// Draw OSC Debug
	// Draw OSC Debug
	if (showDebug) {
		oscManager.drawDebug(ofGetWidth() - 400, 20);
		// helper text
		ofDrawBitmapString("Keys: Space(Play) f(Draw) n(Nav) m(Video) d(Debug) ,(Settings)", 20, ofGetHeight() - 30);
	}

	// Help Overlay — drawn last, on top of everything
	if (showHelp) {
		float sw = (float)ofGetWidth();
		float sh = (float)ofGetHeight();
		ofEnableBlendMode(OF_BLENDMODE_ALPHA);
		ofFill();
		ofSetColor(0, 0, 0, 180);
		ofDrawRectangle(0, 0, sw, sh);

		ofSetColor(255);
		int lx = 60, ly = 60, lineH = 18;
		ofDrawBitmapString("=== KEYBOARD SHORTCUTS ===", lx, ly);
		ly += lineH * 2;
		ofDrawBitmapString("--- Modes ---", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  n     Navigate", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  f     Freehand Draw", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  w     Wander (auto-path from nearest point)", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  b     Browse (click to audition points)", lx, ly);
		ly += lineH * 2;
		ofDrawBitmapString("--- Playback ---", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  Space Toggle play/pause on selected path", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  q     Create sequential path (all points)", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  e     Toggle step mode (timer-driven steps)", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  Up/Dn Adjust speed", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  L/R   Adjust radius (same scale as r+drag)", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  + / - Increase / decrease sample layer count", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  r / R Increase / decrease catchment radius", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  v / V Increase / decrease playback volume", lx, ly);
		ly += lineH * 2;
		ofDrawBitmapString("--- Path Types & Modes ---", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  q     Create Sequential path from points", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  e     Toggle step mode (Sequential paths)", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  j     Toggle jitter mode (probabilistic advance)", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  l     Toggle default path mode (LOOP/ONCE)", lx, ly);
		ly += lineH * 2;
		ofDrawBitmapString("--- Scrub & Gesture ---", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  Alt+move    Scrub position (X) & volume (Y)", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  Alt+drag    Record gesture (position+volume)", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  g           Clear gesture on selected path", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  v+drag      Adjust volume on browse or selected path", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  r+drag      Adjust radius on selected path", lx, ly);
		ly += lineH * 2;
		ofDrawBitmapString("--- View ---", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  z     Zoom to fit all points", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  Shift+drag  Marquee zoom", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  m       Toggle video view", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  Shift+m Toggle video triggering on selected path", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  ;       Lock/Unlock new video triggers", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  6       Toggle spectrogram layer", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  7       Generate missing thumbnails + spectrograms", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  8       Cycle spectrogram blend (normal/add/multiply/screen/luma)", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  9       Toggle audio visual feedback (onset/energy)", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  t       Toggle text labels", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  a       Fade points and paths in/out", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  d     Toggle debug info", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  ,     Toggle settings panel", lx, ly);
		ly += lineH * 2;
		ofDrawBitmapString("  p     Toggle secondary projector fullscreen Window", lx, ly);
		ly += lineH * 2;
		ofDrawBitmapString("--- Paths ---", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  c     Deselect path", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  Del   Delete selected path", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  x     Clear all paths & points", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  k     Refresh path", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  s     Save current Composition to JSON", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  o     Load Points or Composition from JSON", lx, ly);
		ly += lineH * 2;
		ofDrawBitmapString("--- Annotations ---", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  Tab       Toggle annotation mode (draw callout labels)", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  Shift+Tab Toggle annotation visibility", lx, ly);
		ly += lineH;
		ofDrawBitmapString("  0         Pull all annotation labels inside point bounds", lx, ly);
		ly += lineH * 2;
		ofDrawBitmapString("Press H to close", lx, ly);
		ofDisableBlendMode();
	}

	// Draw mouse cursor crosshair (visible on both main and projector windows)
	{
		float mouseX = ofGetMouseX();
		float mouseY = ofGetMouseY();
		float crosshairSize = 12.0f; // Half-length of crosshair arms
		
		ofSetColor(255, 255, 255, 200); // White with slight transparency
		ofSetLineWidth(2.0f);
		ofNoFill();
		
		// Draw a + (plus) crosshair
		ofDrawLine(mouseX - crosshairSize, mouseY, mouseX + crosshairSize, mouseY); // Horizontal
		ofDrawLine(mouseX, mouseY - crosshairSize, mouseX, mouseY + crosshairSize); // Vertical
		
		// Optional: draw a small circle around the center
		ofDrawCircle(mouseX, mouseY, 4.0f);
		
		ofSetLineWidth(1.0f); // Reset
	}

	if (isLoadOverlayOpen) {
		ofEnableBlendMode(OF_BLENDMODE_ALPHA);
		ofSetColor(0, 0, 0, 255);
		ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());

		float panelW = std::min(760.0f, ofGetWidth() * 0.86f);
		float rowH = 24.0f;
		int visibleRows = 18;
		int entryCount = (int)loadOverlayEntries.size();
		int rowsToDraw = std::min(visibleRows, std::max(1, entryCount));
		float panelH = 136.0f + rowH * rowsToDraw;
		float panelX = (ofGetWidth() - panelW) * 0.5f;
		float panelY = (ofGetHeight() - panelH) * 0.5f;

		ofSetColor(245, 245, 245, 255);
		ofDrawRectangle(panelX, panelY, panelW, panelH);
		ofNoFill();
		ofSetColor(20, 20, 20, 255);
		ofSetLineWidth(1.0f);
		ofDrawRectangle(panelX, panelY, panelW, panelH);
		ofFill();

		ofSetColor(10, 10, 10, 255);
		ofDrawBitmapString("Open JSON (non-modal browser)", panelX + 16, panelY + 24);
		ofSetColor(40, 40, 40, 255);
		ofDrawBitmapString("Enter: open/load   Up/Down: select   Backspace/Delete: parent   O: close", panelX + 16, panelY + 44);

		std::string dirLabel = loadOverlayCurrentDir;
		if (dirLabel.size() > 96) {
			dirLabel = "..." + dirLabel.substr(dirLabel.size() - 93);
		}
		ofSetColor(20, 20, 20, 255);
		ofDrawBitmapString(dirLabel, panelX + 16, panelY + 64);

		float listY = panelY + 84.0f;
		if (loadOverlayEntries.empty()) {
			ofSetColor(220, 130, 130);
			ofDrawBitmapString("No folders or JSON files found here.", panelX + 16, listY + 18);
		} else {
			int startIndex = loadOverlayScrollOffset;
			int endIndex = std::min(startIndex + visibleRows, entryCount);
			for (int i = startIndex; i < endIndex; ++i) {
				int row = i - startIndex;
				float rowY = listY + row * rowH;
				if (i == loadOverlaySelectedIndex) {
					ofSetColor(90, 130, 190, 255);
					ofDrawRectangle(panelX + 8, rowY, panelW - 16, rowH - 2);
				}

				ofSetColor(20, 20, 20, 255);
				std::string label = ofFilePath::getFileName(loadOverlayEntries[i]);
				std::string parentPath = getParentDirectoryPath(loadOverlayCurrentDir);
				if (!parentPath.empty() && loadOverlayEntryIsDirectory[i] && loadOverlayEntries[i] == parentPath) {
					label = "..";
				}
				if (loadOverlayEntryIsDirectory[i]) {
					if (label.empty()) label = loadOverlayEntries[i];
					label = "[DIR] " + label;
				}
				if (label.size() > 88) {
					label = label.substr(0, 85) + "...";
				}
				ofDrawBitmapString(label, panelX + 16, rowY + 16);
			}

			if (entryCount > visibleRows) {
				ofSetColor(40, 40, 40, 255);
				ofDrawBitmapString(ofToString(loadOverlaySelectedIndex + 1) + "/" + ofToString(entryCount), panelX + panelW - 90, panelY + panelH - 10);
			}
		}
	}
}

//--------------------------------------------------------------
void ofApp::draw() {
	drawVisuals();

	// Draw GUI only on the main control window
	if (showGui) {
		gui.draw();
	}
}

//--------------------------------------------------------------
void ofApp::drawProjector(ofEventArgs & args) {
	drawVisuals();
}

void ofApp::exit() {
	gui.saveToFile("settings.xml");
	annotationManager.saveToFile("annotations.json");
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key) {
	inputManager.updateModifiers(key, true);

	if (isLoadOverlayOpen) {
		if (key == 'o' || key == 'O') {
			closeLoadOverlay();
			return;
		}

		if (key == OF_KEY_BACKSPACE || key == OF_KEY_DEL || key == OF_KEY_LEFT || key == 127 || key == 8 || key == 63272 || key == 65535) {
			std::string parent = getParentDirectoryPath(loadOverlayCurrentDir);
			if (!parent.empty() && parent != loadOverlayCurrentDir) {
				openLoadOverlayPath(parent);
			}
			return;
		}

		if (key == OF_KEY_UP && !loadOverlayEntries.empty()) {
			loadOverlaySelectedIndex--;
			if (loadOverlaySelectedIndex < 0) {
				loadOverlaySelectedIndex = (int)loadOverlayEntries.size() - 1;
			}
			if (loadOverlaySelectedIndex < loadOverlayScrollOffset) {
				loadOverlayScrollOffset = loadOverlaySelectedIndex;
			}
			return;
		}

		if (key == OF_KEY_DOWN && !loadOverlayEntries.empty()) {
			loadOverlaySelectedIndex = (loadOverlaySelectedIndex + 1) % (int)loadOverlayEntries.size();
			int visibleRows = 18;
			if (loadOverlaySelectedIndex >= loadOverlayScrollOffset + visibleRows) {
				loadOverlayScrollOffset = loadOverlaySelectedIndex - visibleRows + 1;
			}
			return;
		}

		if ((key == OF_KEY_RETURN || key == '\n' || key == '\r' || key == OF_KEY_RIGHT) && !loadOverlayEntries.empty()) {
			std::string chosen = loadOverlayEntries[loadOverlaySelectedIndex];
			if (loadOverlayEntryIsDirectory[loadOverlaySelectedIndex]) {
				openLoadOverlayPath(chosen);
			} else {
				closeLoadOverlay();
				loadCompositionOrPoints(chosen);
			}
			return;
		}

		return;
	}

	// Annotation system gets first crack — when typing, it consumes all keys
	if (annotationManager.onKeyPressed(key, points)) return;

	if (key == '5' || key == '%') {
		pointGlyphMode = nextPointGlyphMode(pointGlyphMode);
		pointGlyphMode_param = (int)pointGlyphMode;
		ofLogNotice("ofApp") << "Point Glyph Mode: " << pointGlyphModeName(pointGlyphMode);
		return;
	}
	if (key == '6' || key == '^') {
		spectrogramLayerEnabled = !spectrogramLayerEnabled;
		ofLogNotice("ofApp") << "Spectrogram Layer: " << (spectrogramLayerEnabled ? "ON" : "OFF");
		return;
	}
	if (key == '7' || key == '&') {
		beginMediaAssetGeneration();
		return;
	}
	if (key == '8' || key == '*') {
		spectrogramBlendMode = nextSpectrogramBlendMode(spectrogramBlendMode);
		spectrogramBlendMode_param = (int)spectrogramBlendMode;
		ofLogNotice("ofApp") << "Spectrogram Blend Mode: "
			<< spectrogramBlendModeName(spectrogramBlendMode);
		return;
	}
	if (key == '9' || key == '(') {
		audioVisualFeedbackEnabled = !audioVisualFeedbackEnabled.get();
		ofLogNotice("ofApp") << "Audio Visual Feedback: "
			<< (audioVisualFeedbackEnabled.get() ? "ON" : "OFF");
		return;
	}

	// Pull all labels inside point bounds
	if (key == '0') {
		annotationManager.clampAllLabelsToPointsBounds(points);
		ofLogNotice("AnnotationManager") << "Pulled annotation labels inside point bounds.";
		return;
	}

	// Delegate to InputManager for command mapping
	AppCommand cmd = inputManager.getCommandForKey(key);

	switch (cmd) {
	case CMD_MODE_DRAW:
		currentMode = DRAW_FREEHAND;
		isDrawingPath = false; // Reset
		break;

	case CMD_MODE_NAVIGATE:
		currentMode = NAVIGATE;
		break;

	case CMD_MODE_WANDER:
		currentMode = WANDER;
		break;

	case CMD_TOGGLE_PLAYBACK: // Spacebar
		if (selectedPath) {
			selectedPath->togglePlayback();
			if (!selectedPath->isActive) {
				stopPathSamples(selectedPath);
			}
		}
		break;

	case CMD_TOGGLE_GLOBAL_PLAYBACK: // Return/Enter
	{
		bool anyPlaying = false;
		for (auto & p : paths) {
			if (p->isActive) {
				anyPlaying = true;
				break;
			}
		}

		for (auto & p : paths) {
			if (anyPlaying) {
				p->isActive = false;
				stopPathSamples(p);
			} else {
				p->isActive = true;
			}
		}
	} break;

	case CMD_LOAD_POINTS: // 'o'
	{
		closeLoadOverlay();
		ofFileDialogResult result = ofSystemLoadDialog("Select JSON Data or Composition File");
		if (result.bSuccess) {
			loadCompositionOrPoints(result.getPath());
		}
	} break;

	case CMD_SAVE_COMPOSITION: // 's'
	{
		string newTitle = ofSystemTextBoxDialog("Enter Composition Title", compositionTitle);
		// If user cancels, it might return empty or the original. We will assume if they entered something we use it.
		// Actually, ofSystemTextBoxDialog returns the entered string, or empty if cancelled/empty.
		compositionTitle = newTitle;
		ofFileDialogResult result = ofSystemSaveDialog("konvolute-01.json", "Save Konvolute As");
		if (result.bSuccess) {
			saveComposition(result.getPath());
		}
	} break;

	case CMD_TOGGLE_VIDEO:
		if (key == 'm' || key == 'M') {
			showVideo = !showVideo;
		}
		if (!showVideo) videoFront->stop();
		break;

	case CMD_TOGGLE_FULLSCREEN_PROJECTOR:
		if (projectorWindow) {
			// Check if the window is actually still open and valid
			auto glfwWin = std::dynamic_pointer_cast<ofAppGLFWWindow>(projectorWindow);
			if (glfwWin && glfwWin->getGLFWWindow() != nullptr && !glfwWin->getWindowShouldClose()) {
				projectorWindow->toggleFullscreen();
			} else {
				// Window was closed by the user, clean up the pointer and recreate it
				ofRemoveListener(projectorWindow->events().draw, this, &ofApp::drawProjector);
				projectorWindow.reset();
			}
		}

		if (!projectorWindow) {
			// Create the projector window dynamically
			ofGLFWWindowSettings settings;
			settings.setSize(1024, 768);
			settings.setPosition(ofVec2f(1500, 100)); // Default offset for secondary display
			settings.resizable = true;
			settings.shareContextWith = mainWindow;

			projectorWindow = ofCreateWindow(settings);
			ofAddListener(projectorWindow->events().draw, this, &ofApp::drawProjector);
		}
		break;

	case CMD_TOGGLE_TEXT:
		showText = !showText;
		break;

	case CMD_TOGGLE_TITLE:
		showTitle = !showTitle;
		break;

	case CMD_TOGGLE_DATA_VISIBILITY:
		targetDataVisualAlpha = (targetDataVisualAlpha > 0.5f) ? 0.0f : 1.0f;
		break;

	case CMD_TOGGLE_PINGPONG:
		if (selectedPath) {
			selectedPath->isPingPong = !selectedPath->isPingPong;
		} else if (currentPath) {
			currentPath->isPingPong = !currentPath->isPingPong;
		}
		break;

	case CMD_TOGGLE_JITTER:
		if (selectedPath) {
			selectedPath->jitterMode = !selectedPath->jitterMode;
			ofLogNotice("ofApp") << "Jitter mode for path " << selectedPath->name << " set to: " << (selectedPath->jitterMode ? "ON" : "OFF");
		}
		break;

	case CMD_TOGGLE_SETTINGS:
		showGui = !showGui;
		break;

	case CMD_TOGGLE_BROWSE:
		if (currentMode == BROWSE)
			currentMode = NAVIGATE;
		else
			currentMode = BROWSE;

		// Stop any playing sound when switching
		if (hasLastHoveredPoint) {
			oscManager.stopSample(mediaRoot + lastHoveredPoint.filename, "path-0");
			hasLastHoveredPoint = false;
			hasHoveredPoint = false;
		}
		break;

	case CMD_REFRESH_PATH:
		if (selectedPath) {
			oscManager.sendPathRefresh(selectedPath->name);
		} else {
			oscManager.sendPathRefresh("path-0");
		}
		break;

	case CMD_INCREASE_RADIUS: // 'r'
		bHoldingR = true;
		break;

	case CMD_CLOUD_LOCAL:
		currentCloudMode = PointCloudMode::LOCAL;
		ofLogNotice("ofApp") << "Switched to LOCAL point cloud";
		annotationManager.refreshAnchors(points);
		break;

	case CMD_CLOUD_MID:
		currentCloudMode = PointCloudMode::MID;
		ofLogNotice("ofApp") << "Switched to MID point cloud";
		annotationManager.refreshAnchors(points);
		break;

	case CMD_CLOUD_GLOBAL:
		currentCloudMode = PointCloudMode::GLOBAL;
		ofLogNotice("ofApp") << "Switched to GLOBAL point cloud";
		annotationManager.refreshAnchors(points);
		break;

	case CMD_CYCLE_THIRD_DIM:
		switch (currentThirdDimMode) {
		case ThirdDimMode::NONE:
			currentThirdDimMode = ThirdDimMode::INSTABILITY;
			ofLogNotice("ofApp") << "Third Dim: INSTABILITY";
			break;
		case ThirdDimMode::INSTABILITY:
			currentThirdDimMode = ThirdDimMode::ATTACK;
			ofLogNotice("ofApp") << "Third Dim: ATTACK";
			break;
		case ThirdDimMode::ATTACK:
			currentThirdDimMode = ThirdDimMode::BRIGHTNESS;
			ofLogNotice("ofApp") << "Third Dim: BRIGHTNESS";
			break;
		case ThirdDimMode::BRIGHTNESS:
			currentThirdDimMode = ThirdDimMode::NONE;
			ofLogNotice("ofApp") << "Third Dim: NONE";
			break;
		}
		break;

	case CMD_CLUSTER_PREV:
		if (!sortedClusterIds.empty()) {
			if (activeClusterId == -999) {
				activeClusterId = sortedClusterIds.back();
			} else {
				auto it = std::find(sortedClusterIds.begin(), sortedClusterIds.end(), activeClusterId);
				if (it == sortedClusterIds.end() || it == sortedClusterIds.begin()) {
					activeClusterId = sortedClusterIds.back();
				} else {
					--it;
					activeClusterId = *it;
				}
			}
			std::string label = (clusters.count(activeClusterId) && !clusters[activeClusterId].label.empty())
				? clusters[activeClusterId].label : "id " + ofToString(activeClusterId);
			ofLogNotice("ofApp") << "Cluster filter: " << label;
		}
		break;

	case CMD_CLUSTER_NEXT:
		if (!sortedClusterIds.empty()) {
			if (activeClusterId == -999) {
				activeClusterId = sortedClusterIds.front();
			} else {
				auto it = std::find(sortedClusterIds.begin(), sortedClusterIds.end(), activeClusterId);
				if (it == sortedClusterIds.end() || std::next(it) == sortedClusterIds.end()) {
					activeClusterId = sortedClusterIds.front();
				} else {
					++it;
					activeClusterId = *it;
				}
			}
			std::string label = (clusters.count(activeClusterId) && !clusters[activeClusterId].label.empty())
				? clusters[activeClusterId].label : "id " + ofToString(activeClusterId);
			ofLogNotice("ofApp") << "Cluster filter: " << label;
		}
		break;

	case CMD_CLUSTER_CLEAR:
		activeClusterId = -999;
		ofLogNotice("ofApp") << "Cluster filter cleared";
		break;

	case CMD_TOGGLE_NEIGHBOUR:
		neighbourModeActive = !neighbourModeActive;
		if (!neighbourModeActive) {
			selectedPointIdx = -1;
			neighbourSeqPlaying = false;
			neighbourQueue.clear();
			neighbourQueueDistances.clear();
			neighbourLastPlayedIdx = -1;
			neighbourLastPlayedMs = 0;
		}
		ofLogNotice("ofApp") << "Neighbour mode: " << (neighbourModeActive ? "ON" : "OFF");
		break;

	case CMD_NEIGHBOUR_PLAY_SEQ:
		if (!neighbourModeActive) {
			showTitle = !showTitle;
		} else if (selectedPointIdx >= 0 && selectedPointIdx < (int)points.size()) {
			// Build queue sorted by ascending distance
			const DataPoint & sel = points[selectedPointIdx];
			int numN = (int)std::min(sel.true_neighbors.size(), sel.true_distances.size());
			// Create index-distance pairs, sort by distance
			std::vector<std::pair<float, int>> distIdx;
			distIdx.reserve(numN);
			for (int ni = 0; ni < numN; ++ni) {
				distIdx.push_back({sel.true_distances[ni], sel.true_neighbors[ni]});
			}
			std::sort(distIdx.begin(), distIdx.end());
			neighbourQueue.clear();
			neighbourQueueDistances.clear();
			for (auto & di : distIdx) {
				if (di.second >= 0 && di.second < (int)points.size()) {
					neighbourQueue.push_back(di.second);
					neighbourQueueDistances.push_back(di.first);
				}
			}
			neighbourSeqIdx = 0;
			neighbourSeqLastTriggerMs = 0; // Trigger immediately on first tick
			neighbourSeqPlaying = true;
			ofLogNotice("ofApp") << "Neighbour seq play: " << neighbourQueue.size() << " neighbours";
		}
		break;

	case CMD_NEIGHBOUR_PLAY_ALL:
		if (!neighbourModeActive) {
			// No previous binding for u/U — do nothing
		} else if (selectedPointIdx >= 0 && selectedPointIdx < (int)points.size()) {
			const DataPoint & sel = points[selectedPointIdx];
			float baseVol = paths.empty() ? 0.5f : paths[0]->volume;
			string mode = paths.empty() ? "once" : paths[0]->getMode();
			int numN = (int)std::min(sel.true_neighbors.size(), sel.true_distances.size());

			float minD = std::numeric_limits<float>::max();
			float maxD = std::numeric_limits<float>::lowest();
			for (int ni = 0; ni < numN; ++ni) {
				int idx = sel.true_neighbors[ni];
				if (idx >= 0 && idx < (int)points.size()) {
					float d = sel.true_distances[ni];
					minD = std::min(minD, d);
					maxD = std::max(maxD, d);
				}
			}
			float range = std::max(maxD - minD, 1e-6f);

			for (int ni = 0; ni < numN; ++ni) {
				int idx = sel.true_neighbors[ni];
				if (idx >= 0 && idx < (int)points.size()) {
					float d = sel.true_distances[ni];
					float nearWeight = 1.0f - ((d - minD) / range); // near=1, far=0
					nearWeight = std::clamp(nearWeight, 0.0f, 1.0f);
					float volScale = ofLerp(0.2f, 1.0f, nearWeight);
					float vol = baseVol * volScale;
					oscManager.sendSample(mediaRoot + points[idx].filename, "path-0", vol, mode);
					noteTriggeredMediaForSpectrogram(mediaRoot + points[idx].filename);
				}
			}
			ofLogNotice("ofApp") << "Neighbour play all: fired " << numN << " samples";
		}
		break;

	case CMD_INCREASE_VOLUME: // 'v'
		bHoldingV = true;
		break;

	case CMD_DECREASE_VOLUME: // 'V'
	{
		auto targetPath = selectedPath ? selectedPath : (!paths.empty() ? paths[0] : nullptr);
		if (targetPath) {
			targetPath->volume -= 0.1f;
			if (targetPath->volume < 0.0f) targetPath->volume = 0.0f;
		}
	} break;

	case CMD_TOGGLE_PATH_VIDEO:
		if (selectedPath) {
			selectedPath->sendToVideo = !selectedPath->sendToVideo;
			ofLogNotice("ofApp") << "Selected path " << selectedPath->name << " video routing set to: " << (selectedPath->sendToVideo ? "ON" : "OFF");
		} else {
			// Find path-0 and toggle it
			for (auto & p : paths) {
				if (p->name == "path-0") {
					p->sendToVideo = !p->sendToVideo;
					ofLogNotice("ofApp") << "Browse path " << p->name << " video routing set to: " << (p->sendToVideo ? "ON" : "OFF");
					break;
				}
			}
		}
		break;

	case CMD_TOGGLE_VIDEO_LOCK: // ';'
		videoTriggerLocked = !videoTriggerLocked;
		ofLogNotice("ofApp") << "Video trigger locked set to: " << (videoTriggerLocked ? "ON" : "OFF");
		break;

	case CMD_DESELECT_PATH: // 'c'
		selectedPath = nullptr;
		break;

	case CMD_DELETE_PATH:
		if (selectedPath) {
			// Stop playing samples
			stopPathSamples(selectedPath);

			// Send OSC Removal
			oscManager.sendPathRemove(selectedPath->name);
			oscManager.sendUIPathRemove(selectedPath->name);

			// Remove from paths vector
			// std::remove moves elements to end and returns iterator to first "removed" element
			auto it = std::remove(paths.begin(), paths.end(), selectedPath);
			paths.erase(it, paths.end());

			selectedPath = nullptr;
		}
		break;

	case CMD_CREATE_SEQUENTIAL_PATH: {
		// Create new path
		auto newPath = std::make_shared<PathObject>(pathIdCounter++);
		newPath->name = "seq-" + ofToString(newPath->id);
		newPath->mode = defaultPathMode;
		newPath->isSequential = true;
		newPath->radius = 1.0f; // Minimal radius as we use step logic

		// Sort point pointers
		std::vector<const DataPoint *> sortedPtrs;
		sortedPtrs.reserve(points.size());
		for (const auto & p : points)
			sortedPtrs.push_back(&p);

		std::sort(sortedPtrs.begin(), sortedPtrs.end(), [](const DataPoint * a, const DataPoint * b) {
			if (a->filename.empty() && b->filename.empty()) return false;
			if (a->filename.empty()) return true;
			if (b->filename.empty()) return false;
			return naturalCompare(a->filename, b->filename) < 0;
		});

		newPath->attachedPoints = sortedPtrs;

		// Build polyline as straight lines — no smoothing for sequential paths
		for (const auto * p : sortedPtrs) {
			newPath->sequentialPoints.push_back(*p);
			newPath->controlPoints.push_back(ofVec2f(p->x, p->y));
			newPath->polyline.addVertex(p->x, p->y, 0);
		}
		// Do not call finalize() — we want straight lines, not a smoothed spline

		oscManager.sendUIPathAdd(newPath->name);
		paths.push_back(newPath);

		// Auto-select
		selectedPath = newPath;
		selectedPath->isSelected = true;
		for (auto & p : paths)
			if (p != selectedPath) p->isSelected = false;
	} break;

	case CMD_CLEAR_ALL: // 'x'
		paths.clear();
		points.clear();
		selectedPath = nullptr;
		if (spatialGrid) spatialGrid->clear();
		oscManager.sendClear();

		// Restore path-0
		{
			auto p0 = std::make_shared<PathObject>(0);
			p0->name = "path-0";
			p0->mode = defaultPathMode;
			p0->isActive = false;
			p0->sendToVideo = false;
			oscManager.sendUIPathAdd(p0->name);
			paths.push_back(p0);
		}
		break;

	case CMD_RESET_ZOOM: // 'z' — zoom to extents of loaded points
		zoomToDataExtents(true);
		break;

	case CMD_TOGGLE_DEBUG: // 'd'
		showDebug = !showDebug;
		ofLogNotice() << "Toggle Debug: " << showDebug;
		break;

	case CMD_CLEAR_GESTURE:
		if (selectedPath) {
			selectedPath->clearGesture();
			ofLogNotice("ofApp") << "Gesture cleared.";
		}
		break;

	case CMD_TOGGLE_STEP_MODE:
		if (selectedPath && selectedPath->isSequential) {
			selectedPath->stepMode = !selectedPath->stepMode;
			selectedPath->stepTimer = 0.0f;
			if (selectedPath->stepMode) selectedPath->currentStepIndex = 0;
			ofLogNotice("ofApp") << "Step mode: " << (selectedPath->stepMode ? "ON" : "OFF");
		}
		break;

	case CMD_TOGGLE_HELP:
		showHelp = !showHelp;
		break;

	case CMD_TOGGLE_DEFAULT_MODE:
		if (defaultPathMode == PathObject::ONCE_MODE) {
			defaultPathMode = PathObject::LOOP_MODE;
		} else {
			defaultPathMode = PathObject::ONCE_MODE;
		}
		if (!paths.empty()) {
			paths[0]->mode = defaultPathMode;
			oscManager.sendUIPathUpdate(paths[0]->id, paths[0]->isActive,
				paths[0]->radius, paths[0]->direction, paths[0]->sampleNum,
				paths[0]->volume, paths[0]->falloff, paths[0]->speed, paths[0]->mode);
		}
		ofLogNotice("ofApp") << "Default Path Mode toggled to: " << (defaultPathMode == PathObject::LOOP_MODE ? "LOOP" : "ONCE");
		break;

	default:
		// Plus / Minus for SampleNum
		if ((key == '+' || key == '=') && selectedPath) {
			selectedPath->sampleNum++;
		} else if ((key == '-' || key == '_') && selectedPath) {
			selectedPath->sampleNum--;
			if (selectedPath->sampleNum < 1) selectedPath->sampleNum = 1;
		}

		// Handle arrow keys manually
		if (!ofGetKeyPressed(OF_KEY_SHIFT)) {
			auto targetPath = selectedPath ? selectedPath : (!paths.empty() ? paths[0] : nullptr);
			if (targetPath) {
				if (key == OF_KEY_LEFT || key == OF_KEY_RIGHT || key == OF_KEY_UP || key == OF_KEY_DOWN) {
					// Match r+drag / v+drag mappings: linear steps in screen-space t,
					// log interpolation in world-parameter space.
					float diag = 0.0f;
					if (!points.empty()) {
						float minX = std::numeric_limits<float>::max();
						float maxX = std::numeric_limits<float>::lowest();
						float minY = std::numeric_limits<float>::max();
						float maxY = std::numeric_limits<float>::lowest();
						for (const auto & p : points) {
							minX = std::min(minX, p.x);
							maxX = std::max(maxX, p.x);
							minY = std::min(minY, p.y);
							maxY = std::max(maxY, p.y);
						}
						diag = ofVec2f(maxX - minX, maxY - minY).length();
					}

					if (diag > 0.0f) {
						float tStep = 0.01f;

						if (key == OF_KEY_LEFT || key == OF_KEY_RIGHT) {
							float minRadius = std::max(diag * 0.001f, 0.001f);
							float maxRadius = std::max(minRadius, diag * 0.5f);
							float ratio = maxRadius / minRadius;

							if (ratio > 1.0f) {
								float radiusClamped = std::clamp(targetPath->radius, minRadius, maxRadius);
								float t = std::log(radiusClamped / minRadius) / std::log(ratio);
								t += (key == OF_KEY_RIGHT) ? tStep : -tStep;
								t = std::clamp(t, 0.0f, 1.0f);
								targetPath->radius = minRadius * std::pow(ratio, t);
							} else {
								targetPath->radius = minRadius;
							}
						}

						if (key == OF_KEY_UP || key == OF_KEY_DOWN) {
							float minSpeed = std::max(diag * 0.00005f, 0.0001f);
							float maxSpeed = std::max(minSpeed, diag * 0.25f);
							float ratio = maxSpeed / minSpeed;

							if (ratio > 1.0f) {
								float speedClamped = std::clamp(targetPath->speed, minSpeed, maxSpeed);
								float t = std::log(speedClamped / minSpeed) / std::log(ratio);
								t += (key == OF_KEY_UP) ? tStep : -tStep;
								t = std::clamp(t, 0.0f, 1.0f);
								targetPath->speed = minSpeed * std::pow(ratio, t);
							} else {
								targetPath->speed = minSpeed;
							}
						}
					}
				}

				oscManager.sendUIPathUpdate(targetPath->id, targetPath->isActive,
					targetPath->radius, targetPath->direction, targetPath->sampleNum,
					targetPath->volume, targetPath->falloff, targetPath->speed, targetPath->mode);
			}
		}

		// Path Cycling (Shift + Arrow)
		if (ofGetKeyPressed(OF_KEY_SHIFT)) {
			if (key == OF_KEY_LEFT) {
				// Cycle Previous
				if (paths.empty()) return;
				int idx = -1;
				for (int i = 0; i < paths.size(); ++i)
					if (paths[i]->isSelected) idx = i;

				if (idx != -1) paths[idx]->isSelected = false;
				int newIdx = (idx - 1 + paths.size()) % paths.size();
				if (newIdx < 0) newIdx = 0; // Should be handled by modulo but safety

				selectedPath = paths[newIdx];
				selectedPath->isSelected = true;
				sendFullUIUpdate(selectedPath);

			} else if (key == OF_KEY_RIGHT) {
				// Cycle Next
				if (paths.empty()) return;
				int idx = -1;
				for (int i = 0; i < paths.size(); ++i)
					if (paths[i]->isSelected) idx = i;

				if (idx != -1) paths[idx]->isSelected = false;
				int newIdx = (idx + 1) % paths.size();

				selectedPath = paths[newIdx];
				selectedPath->isSelected = true;
				sendFullUIUpdate(selectedPath);

			} else if (key == OF_KEY_DOWN) {
				// Deselect All
				for (auto & p : paths)
					p->isSelected = false;
				selectedPath = nullptr;
			}
		}
		break;
	}
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key) {
	inputManager.updateModifiers(key, false);

	if (key == 'r' || key == 'R') bHoldingR = false;
	if (key == 'v' || key == 'V') bHoldingV = false;
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button) {
	if (isLoadOverlayOpen) {
		float panelW = std::min(760.0f, ofGetWidth() * 0.86f);
		float rowH = 24.0f;
		int visibleRows = 18;
		int entryCount = (int)loadOverlayEntries.size();
		int rowsToDraw = std::min(visibleRows, std::max(1, entryCount));
		float panelH = 136.0f + rowH * rowsToDraw;
		float panelX = (ofGetWidth() - panelW) * 0.5f;
		float panelY = (ofGetHeight() - panelH) * 0.5f;

		if (x < panelX || x > panelX + panelW || y < panelY || y > panelY + panelH) {
			closeLoadOverlay();
			return;
		}

		float listY = panelY + 84.0f;
		int startIndex = loadOverlayScrollOffset;
		int endIndex = std::min(startIndex + visibleRows, entryCount);
		for (int i = startIndex; i < endIndex; ++i) {
			int row = i - startIndex;
			float rowTop = listY + row * rowH;
			if (y >= rowTop && y < rowTop + rowH) {
				loadOverlaySelectedIndex = i;
				if (button == 0 && loadOverlaySelectedIndex >= 0 && loadOverlaySelectedIndex < entryCount) {
					std::string chosen = loadOverlayEntries[loadOverlaySelectedIndex];
					if (loadOverlayEntryIsDirectory[loadOverlaySelectedIndex]) {
						openLoadOverlayPath(chosen);
					} else {
						closeLoadOverlay();
						loadCompositionOrPoints(chosen);
					}
				}
				return;
			}
		}
		return;
	}

	// Forward to annotation system first
	if (annotationManager.onMousePressed(glm::vec2(x, y), button)) return;

	// While adjusting radius/speed via drag, consume press and block mode actions.
	if (bHoldingR || bHoldingV) {
		lastMouse.set(x, y);
		isDragging = false;
		isDraggingPath = false;
		isMarqueeZooming = false;
		return;
	}

	ofVec2f worldPos = screenToWorld(x, y);

	if (currentMode == DRAW_FREEHAND) {
		if (!isDrawingPath) {
			currentPath = std::make_shared<PathObject>(pathIdCounter++);
			currentPath->mode = defaultPathMode;
			isDrawingPath = true;
		}
		currentPath->addPoint(worldPos);
	} else {
		// Alt+click in any mode: start gesture recording on selected path
		if (ofGetKeyPressed(OF_KEY_ALT) && selectedPath) {
			selectedPath->gesturePoints.clear();
			selectedPath->hasGesture = false;
			selectedPath->gestureStartTime = ofGetElapsedTimeMillis();
			isRecordingGesture = true;
			return;
		}
	}

	if (currentMode == NAVIGATE) {
		// Marquee Zoom Check (Shift + Click)
		if (ofGetKeyPressed(OF_KEY_SHIFT)) {
			isMarqueeZooming = true;
			marqueeStart.set(x, y);
			marqueeEnd.set(x, y);
		} else {
			lastMouse.set(x, y);
			isDragging = true;
			// Manual pan should take control immediately and cancel any lingering view animation.
			isViewAnimating = false;
			targetPan = pan;
			targetZoom = zoom;

			// Selection Logic
			// Simple closest path selection
			float minDist = 20.0f / zoom;
			selectedPath = nullptr;
			for (auto & p : paths) {
				float d = p->getDistanceToPoint(worldPos);
				if (d < minDist) {
					minDist = d;
					selectedPath = p;
				}
			}

			// Update selection state
			for (auto & p : paths)
				p->isSelected = (p == selectedPath);
		}
	} else if (currentMode == BROWSE) {
		// In BROWSE, click near a path to select+drag it
		if (!ofGetKeyPressed(OF_KEY_ALT)) {
			float minDist = 20.0f / zoom;
			std::shared_ptr<PathObject> clickedPath = nullptr;
			for (auto & p : paths) {
				float d = p->getDistanceToPoint(worldPos);
				if (d < minDist) {
					minDist = d;
					clickedPath = p;
				}
			}
			if (clickedPath) {
				selectedPath = clickedPath;
				for (auto & p : paths)
					p->isSelected = (p == selectedPath);
				isDraggingPath = true;
				lastDragWorld = worldPos;
			}
		}
	} else if (currentMode == WANDER) {
		// Find the nearest DataPoint to the click, build a wandering path from it
		if (!points.empty()) {
			const DataPoint * nearest = nullptr;
			float nearestDist = std::numeric_limits<float>::max();
			for (const auto & p : points) {
				float d = ofVec2f(p.x, p.y).distance(worldPos);
				if (d < nearestDist) {
					nearestDist = d;
					nearest = &p;
				}
			}
			if (nearest != nullptr) {
				auto path = createWanderingPath(*nearest, 50, 0.5f, 3);
				oscManager.sendUIPathAdd(path->name);
				paths.push_back(path);
				// Auto-select
				for (auto & p : paths)
					p->isSelected = false;
				selectedPath = path;
				path->isSelected = true;
				sendFullUIUpdate(path);
			}
		} else {
			ofLogWarning("WANDER") << "No points loaded — cannot create wandering path";
		}
	}

	// Always track last mouse for generic dragging calculations
	lastMouse.set(x, y);
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button) {
	// Forward to annotation system
	if (annotationManager.onMouseDragged(glm::vec2(x, y),
	    glm::vec2(x - lastMouse.x, y - lastMouse.y))) {
		lastMouse.set(x, y);
		return;
	}

	if (bHoldingR || bHoldingV) {
		auto targetPath = selectedPath ? selectedPath : (!paths.empty() ? paths[0] : nullptr);

		// Compute point-cloud extents diagonal in world units.
		float diag = 0.0f;
		if (!points.empty()) {
			float minX = std::numeric_limits<float>::max();
			float maxX = std::numeric_limits<float>::lowest();
			float minY = std::numeric_limits<float>::max();
			float maxY = std::numeric_limits<float>::lowest();
			for (const auto & p : points) {
				minX = std::min(minX, p.x);
				maxX = std::max(maxX, p.x);
				minY = std::min(minY, p.y);
				maxY = std::max(maxY, p.y);
			}
			diag = ofVec2f(maxX - minX, maxY - minY).length();
		}
		if (diag <= 0.0f) {
			lastMouse.set(x, y);
			return;
		}

		// Bottom of screen is minimum, top is maximum.
		float t = std::clamp(1.0f - ((float)y / std::max(1.0f, (float)ofGetHeight())), 0.0f, 1.0f);

		if (targetPath) {
			if (bHoldingR) {
				// Radius uses log interpolation for better control at small values.
				// Max diameter is extents diagonal, so max radius is half of that.
				float minRadius = std::max(diag * 0.001f, 0.001f);
				float maxRadius = std::max(minRadius, diag * 0.5f);
				float ratio = maxRadius / minRadius;
				targetPath->radius = minRadius * std::pow(ratio, t);
			}

			if (bHoldingV) {
				// Max speed is extents diagonal / 4 (world units per frame).
				// Use log interpolation so lower-speed region has finer practical control.
				float minSpeed = std::max(diag * 0.00005f, 0.0001f);
				float maxSpeed = std::max(minSpeed, diag * 0.25f);
				float ratio = maxSpeed / minSpeed;
				targetPath->speed = minSpeed * std::pow(ratio, t);
			}

			oscManager.sendUIPathUpdate(targetPath->id, targetPath->isActive,
				targetPath->radius, targetPath->direction, targetPath->sampleNum,
				targetPath->volume, targetPath->falloff, targetPath->speed, targetPath->mode);
		}

		lastMouse.set(x, y);
		return; // consume drag so it cannot pan/wander/browse-drag
	}

	// Allow gesture recording even in BROWSE mode (before the early return)
	if (ofGetKeyPressed(OF_KEY_ALT) && selectedPath && isRecordingGesture) {
		float scrubPos = std::clamp((float)x / (float)ofGetWidth(), 0.0f, 1.0f);
		float vol = std::clamp(ofMap((float)y, (float)ofGetHeight(), 0.f, 0.f, 1.f), 0.f, 1.f);
		long timeMs = ofGetElapsedTimeMillis() - selectedPath->gestureStartTime;
		selectedPath->gesturePoints.push_back({ scrubPos, vol, timeMs });
		selectedPath->position = scrubPos; // live preview during record
		return;
	}
	// Path dragging in BROWSE mode
	if (currentMode == BROWSE && isDraggingPath && selectedPath) {
		ofVec2f worldPos = screenToWorld(x, y);
		ofVec2f delta = worldPos - lastDragWorld;
		selectedPath->translate(delta);
		lastDragWorld = worldPos;
		return;
	}
	if (currentMode == BROWSE) return;

	ofVec2f worldPos = screenToWorld(x, y);

	// Alt/Option: gesture recording (if started) or scrubbing
	if (ofGetKeyPressed(OF_KEY_ALT) && selectedPath) {
		float scrubPos = std::clamp((float)x / (float)ofGetWidth(), 0.0f, 1.0f);
		selectedPath->position = scrubPos;

		// Sequential path: also advance the step index
		if (selectedPath->isSequential && !selectedPath->sequentialPoints.empty()) {
			int maxIdx = (int)selectedPath->sequentialPoints.size() - 1;
			selectedPath->currentStepIndex = (int)std::clamp(
				ofMap((float)x, 0.f, (float)ofGetWidth(),
					0.f, (float)maxIdx),
				0.f, (float)maxIdx);
		}

		// Y axis → volume (bottom=0, top=1)
		float vol = std::clamp(
			ofMap((float)y, (float)ofGetHeight(), 0.f, 0.f, 1.f), 0.f, 1.f);

		if (isRecordingGesture) {
			// Accumulate gesture point
			long timeMs = ofGetElapsedTimeMillis() - selectedPath->gestureStartTime;
			selectedPath->gesturePoints.push_back({ scrubPos, vol, timeMs });
		} else {
			// Classic scrub: update volume live
			float prevVol = selectedPath->volume;
			selectedPath->volume = vol;
			if (selectedPath->volume != prevVol) {
				oscManager.sendPathVolume(selectedPath->name, selectedPath->volume);
				oscManager.sendUIPathUpdate(selectedPath->id, selectedPath->isActive,
					selectedPath->radius, selectedPath->direction, selectedPath->sampleNum,
					selectedPath->volume, selectedPath->falloff, selectedPath->speed, selectedPath->mode);
			}
		}
		return; // skip normal drag handling
	}

	if (currentMode == DRAW_FREEHAND && isDrawingPath) {
		currentPath->addPoint(worldPos);
	} else if (currentMode == NAVIGATE) {
		if (isMarqueeZooming) {
			marqueeEnd.set(x, y);
		} else if (isDragging) {
			isViewAnimating = false;
			ofVec2f diff = ofVec2f(x, y) - lastMouse;
			pan += diff;
			targetPan = pan;
			targetZoom = zoom;
			lastMouse.set(x, y);
		}
	}
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button) {
	// Forward to annotation system
	if (annotationManager.onMouseReleased(glm::vec2(x, y))) return;

	// Stop path dragging
	isDraggingPath = false;

	// Commit gesture recording if active
	if (isRecordingGesture && selectedPath) {
		selectedPath->hasGesture = !selectedPath->gesturePoints.empty();
		if (selectedPath->hasGesture)
			ofLogNotice("ofApp") << "Gesture recorded: " << selectedPath->gesturePoints.size() << " points.";
		isRecordingGesture = false;
		return;
	}
	if (currentMode == DRAW_FREEHAND && isDrawingPath) {
		// Min Length Check
		if (currentPath->polyline.getPerimeter() < 10.0f / zoom || currentPath->polyline.size() < 3) {
			// Discard
			ofLogNotice() << "Path too short/small, discarding.";
			isDrawingPath = false;
			currentPath = nullptr;
		} else {
			currentPath->finalize();
			oscManager.sendUIPathAdd(currentPath->name);
			paths.push_back(currentPath);

			// Auto-select the new path
			selectedPath = currentPath;
			selectedPath->isSelected = true;
			// Deselect others
			for (auto & p : paths) {
				if (p != selectedPath) p->isSelected = false;
			}
		}

		isDrawingPath = false;
		currentPath = nullptr;
	} else if (currentMode == NAVIGATE && isMarqueeZooming) {
		// Apply Zoom
		float width = abs(marqueeEnd.x - marqueeStart.x);
		float height = abs(marqueeEnd.y - marqueeStart.y);

		if (width > 10 && height > 10) {
			// Calculate center of selection in screen space
			ofVec2f selectionCenter = (marqueeStart + marqueeEnd) / 2.0f;

			// Calculate target zoom to fit the box
			// We want 'width' to become 'screenWidth'
			// zoomFactor = screenWidth / width
			float zoomFactor = MIN(ofGetWidth() / width, ofGetHeight() / height);

			// New Zoom
			float newZoom = zoom * zoomFactor;

			// New Pan
			// We want the world point under 'selectionCenter' to move to screen center
			// currentWorld = screenToWorld(selectionCenter)
			// newScreenCenter = (currentWorld * newZoom) + newPan + centerOffset

			ofVec2f centerOffset(ofGetWidth() / 2, ofGetHeight() / 2);
			ofVec2f targetWorld = screenToWorld(selectionCenter.x, selectionCenter.y);

			// We want targetWorld to be at centerOffset
			// centerOffset = (targetWorld * newZoom) + newPan + centerOffset
			// 0 = (targetWorld * newZoom) + newPan
			// newPan = -(targetWorld * newZoom)

			setViewTarget(newZoom, -(targetWorld * newZoom), true);
		}
		isMarqueeZooming = false;
	}
	isDragging = false;
}

void ofApp::mouseMoved(int x, int y) {
	// Alt/Option scrub — works when key is held (no mouse button needed)
	if (ofGetKeyPressed(OF_KEY_ALT) && selectedPath) {
		if (selectedPath->stepMode && selectedPath->isSequential
			&& !selectedPath->sequentialPoints.empty()) {
			// Snap to discrete step
			int n = (int)selectedPath->sequentialPoints.size();
			int stepIdx = (int)std::clamp(
				ofMap((float)x, 0.f, (float)ofGetWidth(), 0.f, (float)(n - 1)),
				0.f, (float)(n - 1));
			selectedPath->currentStepIndex = stepIdx;
			selectedPath->position = (n > 1) ? (float)stepIdx / (float)(n - 1) : 0.0f;
		} else {
			selectedPath->position = std::clamp((float)x / (float)ofGetWidth(), 0.0f, 1.0f);
			if (selectedPath->isSequential && !selectedPath->sequentialPoints.empty()) {
				int maxIdx = (int)selectedPath->sequentialPoints.size() - 1;
				selectedPath->currentStepIndex = (int)std::clamp(
					ofMap((float)x, 0.f, (float)ofGetWidth(), 0.f, (float)maxIdx),
					0.f, (float)maxIdx);
			}
		}
		// Y axis → volume (bottom=0, top=1) — always
		float prevVol = selectedPath->volume;
		selectedPath->volume = std::clamp(
			ofMap((float)y, (float)ofGetHeight(), 0.f, 0.f, 1.f), 0.f, 1.f);
		if (selectedPath->volume != prevVol) {
			oscManager.sendPathVolume(selectedPath->name, selectedPath->volume);
			oscManager.sendUIPathUpdate(selectedPath->id, selectedPath->isActive,
				selectedPath->radius, selectedPath->direction, selectedPath->sampleNum,
				selectedPath->volume, selectedPath->falloff, selectedPath->speed, selectedPath->mode);
		}
		return;
	}

	// Mouse Scrubbing in Navigate Mode
	/*
	if (currentMode == NAVIGATE) {
		ofVec2f worldPos = screenToWorld(x, y);
		if (spatialGrid) {
			float scrubRadius = 50.0f;
			std::vector<DataPoint> nearby = spatialGrid->findPointsInRadius(worldPos.x, worldPos.y, scrubRadius);
		}
	}
	*/
}
void ofApp::mouseEntered(int x, int y) { }
void ofApp::mouseExited(int x, int y) { }
void ofApp::windowResized(int w, int h) { }
void ofApp::gotMessage(ofMessage msg) { }
void ofApp::dragEvent(ofDragInfo dragInfo) { }

// Helpers
ofVec2f ofApp::screenToWorld(float x, float y) {
	// Inverse transform
	// screen = (world * zoom) + pan + center
	// world * zoom = screen - pan - center
	// world = (screen - pan - center) / zoom

	ofVec2f center(ofGetWidth() / 2, ofGetHeight() / 2);
	return (ofVec2f(x, y) - pan - center) / zoom;
}

void ofApp::setViewTarget(float newZoom, const ofVec2f & newPan, bool animate) {
	newZoom = std::max(newZoom, 0.0001f);
	if (animate) {
		targetZoom = newZoom;
		targetPan = newPan;
		isViewAnimating = true;
	} else {
		zoom = newZoom;
		pan = newPan;
		targetZoom = newZoom;
		targetPan = newPan;
		isViewAnimating = false;
	}
}

void ofApp::updateGridSpacingRangeFromExtents(float minX, float minY, float maxX, float maxY) {
	float width = std::max(0.0f, maxX - minX);
	float height = std::max(0.0f, maxY - minY);
	float diag = std::sqrt(width * width + height * height);
	if (diag <= 0.0f) {
		diag = 1.0f;
	}

	float minSpacing = std::max(diag / 500.0f, 1e-6f);
	float maxSpacing = std::max(diag / 5.0f, minSpacing);

	gridSpacing.setMin(minSpacing);
	gridSpacing.setMax(maxSpacing);

	float clamped = std::clamp((float)gridSpacing.get(), minSpacing, maxSpacing);
	if (std::abs(clamped - gridSpacing.get()) > std::numeric_limits<float>::epsilon()) {
		gridSpacing = clamped;
	}
}

void ofApp::refreshLoadOverlayCandidates() {
	loadOverlayEntries.clear();
	loadOverlayEntryIsDirectory.clear();

	if (loadOverlayCurrentDir.empty()) {
		loadOverlayCurrentDir = ofFilePath::getUserHomeDir();
	}

	ofDirectory dir(loadOverlayCurrentDir);
	if (ofFile(loadOverlayCurrentDir).isFile()) {
		ofFile f(loadOverlayCurrentDir);
		loadOverlayCurrentDir = f.getEnclosingDirectory();
		dir.open(loadOverlayCurrentDir);
	}
	if (!dir.exists()) {
		loadOverlaySelectedIndex = 0;
		loadOverlayScrollOffset = 0;
		return;
	}

	std::string normalizedDir = dir.getAbsolutePath();
	if (!normalizedDir.empty()) {
		if (normalizedDir.back() != '/') normalizedDir += "/";
		loadOverlayCurrentDir = normalizedDir;
	}

	// Parent directory shortcut
	{
		std::string parent = getParentDirectoryPath(loadOverlayCurrentDir);
		if (!parent.empty() && parent != loadOverlayCurrentDir) {
			loadOverlayEntries.push_back(parent);
			loadOverlayEntryIsDirectory.push_back(true);
		}
	}

	dir.listDir();
	std::vector<std::string> directories;
	std::vector<std::string> jsonFiles;
	for (auto & file : dir.getFiles()) {
		if (file.isDirectory()) {
			directories.push_back(file.getAbsolutePath());
		} else if (file.isFile()) {
			std::string ext = ofToLower(ofFilePath::getFileExt(file.getFileName()));
			if (ext == "json") {
				jsonFiles.push_back(file.getAbsolutePath());
			}
		}
	}

	std::sort(directories.begin(), directories.end());
	std::sort(jsonFiles.begin(), jsonFiles.end());

	for (const auto & d : directories) {
		loadOverlayEntries.push_back(d);
		loadOverlayEntryIsDirectory.push_back(true);
	}
	for (const auto & f : jsonFiles) {
		loadOverlayEntries.push_back(f);
		loadOverlayEntryIsDirectory.push_back(false);
	}

	if (loadOverlayEntries.empty()) {
		loadOverlaySelectedIndex = 0;
		loadOverlayScrollOffset = 0;
	} else {
		loadOverlaySelectedIndex = std::clamp(loadOverlaySelectedIndex, 0, (int)loadOverlayEntries.size() - 1);
		loadOverlayScrollOffset = std::clamp(loadOverlayScrollOffset, 0, std::max(0, (int)loadOverlayEntries.size() - 1));
		if (loadOverlaySelectedIndex < loadOverlayScrollOffset) {
			loadOverlayScrollOffset = loadOverlaySelectedIndex;
		}
	}
}

void ofApp::openLoadOverlayPath(const std::string & path) {
	loadOverlayCurrentDir = path;
	loadOverlaySelectedIndex = 0;
	loadOverlayScrollOffset = 0;
	refreshLoadOverlayCandidates();
}

void ofApp::closeLoadOverlay() {
	isLoadOverlayOpen = false;
	loadOverlayEntries.clear();
	loadOverlayEntryIsDirectory.clear();
	loadOverlaySelectedIndex = 0;
	loadOverlayScrollOffset = 0;
}

void ofApp::zoomToDataExtents(bool animate, bool includeAnnotations) {
	if (!points.empty()) {
		float minX = std::numeric_limits<float>::max();
		float maxX = std::numeric_limits<float>::lowest();
		float minY = std::numeric_limits<float>::max();
		float maxY = std::numeric_limits<float>::lowest();
		for (const auto & p : points) {
			if (p.x < minX) minX = p.x;
			if (p.x > maxX) maxX = p.x;
			if (p.y < minY) minY = p.y;
			if (p.y > maxY) maxY = p.y;
		}

		// Include annotation bounds only when explicitly requested and visible.
		if (includeAnnotations && annotationManager.isVisible()) {
			annotationManager.expandWorldBounds(minX, minY, maxX, maxY);
		}

		float dataWidth = (maxX - minX) * 1.2f;
		float dataHeight = (maxY - minY) * 1.2f;
		if (dataWidth <= 0) dataWidth = 1000;
		if (dataHeight <= 0) dataHeight = 1000;
		float newZoom = std::min(ofGetWidth() / dataWidth, ofGetHeight() / dataHeight);
		float centerX = (minX + maxX) / 2.0f;
		float centerY = (minY + maxY) / 2.0f;
		setViewTarget(newZoom, ofVec2f(-centerX * newZoom, -centerY * newZoom), animate);
	} else {
		setViewTarget(1.0f, ofVec2f(0, 0), animate);
	}
}

//--------------------------------------------------------------
bool ofApp::loadPoints(string jsonPath, bool loadGlobalAnnotations) {
	// Keep track of the currently loaded file for saving compositions
	lastLoadedPointsPath = jsonPath;

	// Reset state and clear UI / Synths
	paths.clear();
	selectedPath = nullptr;
	oscManager.sendReset(); // Notify SuperCollider of standard reset
	oscManager.sendClear();

	ofxJSONElement json;
	if (json.open(jsonPath)) {
		bool isLegacyArray = json.isArray();
		if (!json.isObject() && !isLegacyArray) {
			ofLogError("ofApp::loadPoints") << "Error: Expected JSON object with 'points' and 'clusters', or legacy array.";
			ofSystemAlertDialog("The selected file is not a valid points file.\nPlease ensure you select the raw points JSON and not a composition file.");
			return false;
		}

		// Ensure it is not a composition file (which has "points_file")
		if (json.isObject() && json.isMember("points_file")) {
			ofLogError("ofApp::loadPoints") << "Error: Attempted to load composition file as raw points data.";
			return false;
		}

		points.clear();
		clusters.clear();
		thumbnailCache.clear();
		thumbnailCacheLastUsed.clear();
		thumbnailPathByFilename.clear();
		spectrogramCache.clear();
		spectrogramDisplayCache.clear();
		spectrogramPathByMedia.clear();
		currentSpectrogramImagePath.clear();
		spectrogramTrailImagePaths.clear();

		ofLogNotice("ofApp::loadPoints") << "Loading points from " << jsonPath;

		// Intelligent Media Path Resolution
		ofFile file(jsonPath);
		string dir = file.getEnclosingDirectory();
		ofDirectory segDir(dir + "segments");
		if (segDir.exists()) {
			mediaRoot = segDir.getAbsolutePath() + "/";
			ofLogNotice("ofApp::loadPoints") << "Found segments directory. Media Root: " << mediaRoot;
		} else {
			mediaRoot = ""; // Assume bin/data or relative
			ofLogWarning("ofApp::loadPoints") << "Segments directory not found at " << dir << "segments";
		}

		float minX = std::numeric_limits<float>::max();
		float maxX = std::numeric_limits<float>::lowest();
		float minY = std::numeric_limits<float>::max();
		float maxY = std::numeric_limits<float>::lowest();

		const Json::Value * pointsJson = nullptr;

		if (isLegacyArray) {
			pointsJson = &json;
		} else {
			if (json.isMember("clusters") && json["clusters"].isObject()) {
				std::vector<std::string> clusterKeys = json["clusters"].getMemberNames();
				for (size_t i = 0; i < clusterKeys.size(); ++i) {
					const Json::Value & c = json["clusters"][clusterKeys[i]];
					if (!c.isObject()) {
						continue;
					}
					ClusterInfo ci;
					ci.id = c.isMember("id") ? c["id"].asInt() : 0;
					ci.label = c.isMember("label") ? c["label"].asString() : "";
					ci.member_count = c.isMember("member_count") ? c["member_count"].asInt() : 0;
					clusters[ci.id] = ci;
				}
			}
			if (json.isMember("points") && (json["points"].isArray() || json["points"].isObject())) {
				pointsJson = &json["points"];
			}
		}

		if (pointsJson && pointsJson->isArray()) {
			for (Json::ArrayIndex i = 0; i < pointsJson->size(); ++i) {
				const Json::Value & pt = (*pointsJson)[i];
				if (!pt.isObject()) {
					ofLogWarning("ofApp::loadPoints") << "Skipping non-object point at index " << i;
					continue;
				}
				float x = pt["x"].asFloat();
				float y = pt["y"].asFloat();
				string filename = pt.isMember("file") ? pt["file"].asString() : (pt.isMember("filename") ? pt["filename"].asString() : "");
				string text = pt.isMember("text") ? pt["text"].asString() : "";

				DataPoint p(x, y, filename, text);

				auto parseVec2Member = [&](const Json::Value & pointObj, const char * key, const ofVec2f & fallback) {
					if (!pointObj.isMember(key)) {
						return fallback;
					}
					const Json::Value & v = pointObj[key];
					if (!v.isArray() || v.size() < 2) {
						ofLogWarning("ofApp::loadPoints") << "Point " << i << " has invalid '" << key << "' (expected [x,y]); using fallback";
						return fallback;
					}
					return ofVec2f(v[0].asFloat(), v[1].asFloat());
				};

				// Parse new fields
				p.pos_local = parseVec2Member(pt, "pos_local", ofVec2f(x, y));
				p.pos_mid = parseVec2Member(pt, "pos_mid", ofVec2f(x, y));
				p.pos_global = parseVec2Member(pt, "pos_global", ofVec2f(x, y));

				if (pt.isMember("instability")) p.instability = pt["instability"].asFloat();
				if (pt.isMember("cluster_id")) p.cluster_id = pt["cluster_id"].asInt();
				if (pt.isMember("cluster_membership")) p.cluster_membership = pt["cluster_membership"].asFloat();
				if (pt.isMember("attack")) p.attack = pt["attack"].asFloat();
				if (pt.isMember("brightness")) p.brightness = pt["brightness"].asFloat();

				if (pt.isMember("true_neighbors") && pt["true_neighbors"].isArray()) {
					const Json::Value & neighborsArr = pt["true_neighbors"];
					for (Json::ArrayIndex j = 0; j < neighborsArr.size(); ++j) {
						if (neighborsArr[(int)j].isInt() || neighborsArr[(int)j].isNumeric()) {
							p.true_neighbors.push_back(neighborsArr[(int)j].asInt());
						}
					}
				}
				if (pt.isMember("true_distances") && pt["true_distances"].isArray()) {
					const Json::Value & distancesArr = pt["true_distances"];
					for (Json::ArrayIndex j = 0; j < distancesArr.size(); ++j) {
						if (distancesArr[(int)j].isNumeric()) {
							p.true_distances.push_back(distancesArr[(int)j].asFloat());
						}
					}
				}

				// Initialize render position to the currently selected cloud so auto-fit
				// matches what will actually be shown after load.
				switch (currentCloudMode) {
				case PointCloudMode::LOCAL:
					p.x = p.pos_local.x;
					p.y = p.pos_local.y;
					break;
				case PointCloudMode::MID:
					p.x = p.pos_mid.x;
					p.y = p.pos_mid.y;
					break;
				case PointCloudMode::GLOBAL:
					p.x = p.pos_global.x;
					p.y = p.pos_global.y;
					break;
				}

				points.push_back(p);

				if (p.x < minX) minX = p.x;
				if (p.x > maxX) maxX = p.x;
				if (p.y < minY) minY = p.y;
				if (p.y > maxY) maxY = p.y;
			}
		}

		ofLogNotice("ofApp::loadPoints") << "Loaded " << points.size() << " points.";
		ofLogNotice("ofApp::loadPoints") << "Bounds: [" << minX << ", " << minY << "] to [" << maxX << ", " << maxY << "]";

		if (!points.empty()) {
			updateGridSpacingRangeFromExtents(minX, minY, maxX, maxY);
		}

		// Auto-frame
		if (!points.empty()) {
			float dataWidth = maxX - minX;
			float dataHeight = maxY - minY;

			// If points are all at 0,0 or very generic, don't zoom weirdly
			if (dataWidth <= 0) dataWidth = 1000;
			if (dataHeight <= 0) dataHeight = 1000;

			// Add padding (10%)
			dataWidth *= 1.2f;
			dataHeight *= 1.2f;

			float screenW = ofGetWidth();
			float screenH = ofGetHeight();

			float zoomX = screenW / dataWidth;
			float zoomY = screenH / dataHeight;
			float newZoom = std::min(zoomX, zoomY);

			// Center
			float centerX = (minX + maxX) / 2.0f;
			float centerY = (minY + maxY) / 2.0f;

			// Pan logic:
			// screenCenter = (worldCenter * zoom) + pan + centerOffset
			// We want worldCenter to be at screen center (centerOffset)
			// centerOffset = (worldCenter * zoom) + pan + centerOffset
			// 0 = (worldCenter * zoom) + pan
			// pan = -(worldCenter * zoom)

			// Wait, my screenToWorld logic:
			// screen = (world * zoom) + pan + center
			// If we want world point (centerX, centerY) to be at screen center (width/2, height/2):
			// center = (centerWorld * zoom) + pan + center
			// 0 = (centerWorld * zoom) + pan
			// pan = -(centerWorld * zoom)

			ofVec2f newPan(-centerX * newZoom, -centerY * newZoom);
			setViewTarget(newZoom, newPan, false);

			ofLogNotice("ofApp::loadPoints") << "Auto-framed. Target zoom: " << newZoom << " Target pan: " << newPan;
		}

		// Rebuild grid
		spatialGrid = std::make_shared<SpatialGrid>(points, 50.0f / zoom); // Adjust grid cell size based on zoom? Or keep world units?
		// SpatialGrid uses world units. 50.0f is arbitrary. Let's keep it 50 or adapt to density.

		// Build sorted cluster ID list for stepping
		sortedClusterIds.clear();
		for (const auto & kv : clusters) {
			sortedClusterIds.push_back(kv.first);
		}
		std::sort(sortedClusterIds.begin(), sortedClusterIds.end());
		activeClusterId = -999; // Reset filter on new load

		// Thumbnail cache sizing (manual generation is triggered by keypress).
		if (!mediaRoot.empty() && ofDirectory(mediaRoot + "video_segments/").exists()) {
			std::unordered_set<std::string> uniqueBaseNames;
			for (const auto & p : points) {
				std::string baseName = ofFilePath::removeExt(p.filename);
				if (!baseName.empty()) uniqueBaseNames.insert(baseName);
			}

			thumbnailCacheBudget = std::clamp((size_t)(uniqueBaseNames.size() + 64), (size_t)256, (size_t)4096);
			thumbnailLoadsPerFrame = std::clamp((int)(uniqueBaseNames.size() / 24) + 8, 8, 64);
			ofLogNotice("ofApp::loadPoints")
				<< "Media caches configured. Unique media: " << uniqueBaseNames.size()
				<< ", CacheBudget: " << thumbnailCacheBudget
				<< ", LoadsPerFrame: " << thumbnailLoadsPerFrame
				<< ". Press '/' to generate missing thumbnails/spectrograms.";
		}

		// Legacy global annotations are optional and intentionally disabled when
		// loading compositions so annotation state comes only from the composition file.
		if (loadGlobalAnnotations) {
			annotationManager.loadFromFile("annotations.json", points);
		} else {
			annotationManager.clearAnnotations();
		}

		// Register cluster annotations for any labelled clusters
		for (const auto & kv : clusters) {
			if (!kv.second.label.empty()) {
				annotationManager.addClusterAnnotation(kv.second.id, kv.second.label, points);
			}
		}

		// Final safety pass: ensure every loaded/generated annotation label is
		// fully inside the current point bounds.
		annotationManager.clampAllLabelsToPointsBounds(points);

		return true;
	} else {
		ofLogError("ofApp::loadPoints") << "Failed to open JSON file: " << jsonPath;
		return false;
	}
}

void ofApp::sendOscMessage(string addr, float val) {
	ofxOscMessage m;
	m.setAddress(addr);
	m.addFloatArg(val);
	oscManager.sendMessage(m);
}

void ofApp::triggerVideo(const DataPoint & p) {
	if (videoTriggerLocked) return;

	// Drop-frame guard: if the app is lagging (last frame took > 40ms, i.e. <25fps),
	// skip this trigger entirely rather than adding decoder load.
	// This keeps playback timing smooth at the cost of skipping some clips.
	static constexpr float kLagThresholdSec = 1.0f / 25.0f;
	if (ofGetLastFrameTime() > kLagThresholdSec) return;

	// Rate limited video switching
	long now = ofGetElapsedTimeMillis();
	if (videoDisplayMode.get() == 3) {
		// Additional cap for mapped mode to reduce decoder churn.
		if (now - lastMappedVideoSwitchTime < 120) return;
		lastMappedVideoSwitchTime = now;
	}
	if (now - lastVideoSwitchTime > 50) { // 50ms limit
		lastVideoSwitchTime = now;

		string videoPath = "";
		string baseName = ofFilePath::removeExt(p.filename);

		if (mediaRoot != "") {
			string nestedPath = mediaRoot + "video_segments/" + baseName + ".mp4";
			lastAttemptedVideoPath = nestedPath;
			if (ofFile(nestedPath).exists()) {
				videoPath = nestedPath;
			} else {
				lastAttemptedVideoPath += " (Missing)";
				videoPath = mediaRoot + baseName + ".mp4";
			}
		} else {
			videoPath = baseName + ".mp4";
			lastAttemptedVideoPath = videoPath;
		}

		ofFile vFile(videoPath);
		string currentPath = videoFront->isLoaded() ? videoFront->getMoviePath() : "";
		if (vFile.exists() && currentPath != videoPath) {
			if (videoDisplayMode.get() == 0) {
				// Swap: old front keeps running as back — no reload, no flash
				std::swap(videoFront, videoBack);
				videoFront->load(videoPath);
				videoFront->setLoopState(OF_LOOP_NORMAL);
				videoFront->play();
				videoFront->setVolume(0);
				videoAlpha = 0.0f;
				videoAlphaTarget = 255.0f;
			} else if (videoDisplayMode.get() == 2) {
				// GHOST MODE: evict the oldest layer, push a new clip in at front.
				auto evicted = ghostPlayers.back();
				evicted->stop();
				evicted->close();
				ghostPlayers.pop_back();
				evicted->load(videoPath);
				evicted->setLoopState(OF_LOOP_NORMAL);
				evicted->play();
				evicted->setVolume(0);
				ghostPlayers.push_front(evicted);
			} else if (videoDisplayMode.get() == 3) {
				// MAPPED MODE: recycle oldest at front; append newest to back.
				if (mappedPlayers.empty()) {
					MappedClip mc;
					mc.player = std::make_shared<ofVideoPlayer>();
					mappedPlayers.push_back(mc);
				}

				auto evicted = mappedPlayers.front();
				evicted.player->stop();
				mappedPlayers.pop_front();

				evicted.point = p;
				evicted.player->load(videoPath);
				evicted.player->setLoopState(OF_LOOP_NORMAL);
				evicted.player->play();
				evicted.player->setVolume(0);
				mappedPlayers.push_back(evicted);
			} else {
				videoQueue.push_back(videoPath);
			}
		}
	}
}

void ofApp::sendFullUIUpdate(std::shared_ptr<PathObject> p) {
	oscManager.sendUIPathFullUpdate(
		p->id, p->isActive, p->radius, p->direction, p->sampleNum,
		p->volume, p->falloff, p->speed, p->mode,
		// granular
		p->gRateMin, p->gRateMax,
		p->gDurMin, p->gDurMax,
		p->gGrainRateMin, p->gGrainRateMax,
		p->gPosMin, p->gPosMax,
		p->gRand, p->granularMode, p->gEnv,
		// effects
		p->fxEnabled, p->fxReverb, p->fxDelay,
		p->fxDistortion, p->fxCompressor, p->fxFilter,
		// pulser
		p->pulserRateMin, p->pulserRateMax, p->pulserRateRand,
		p->pulserAttack, p->pulserRelease, p->pulserMix);
}

// ------------------------------------------------------------------
// createWanderingPath
// Ported directly from Processing's createWanderingPath():
//   - start: DataPoint where the path begins
//   - maxPoints: maximum number of points to visit
//   - randomness: 0=always closest; 1=always random from pool
//   - numNeighbors: size of the candidate pool to pick from
// ------------------------------------------------------------------
std::shared_ptr<PathObject> ofApp::createWanderingPath(
	const DataPoint & start, int maxPoints, float randomness, int numNeighbors) {
	auto newPath = std::make_shared<PathObject>(pathIdCounter++);
	newPath->isWander = true;
	newPath->mode = defaultPathMode;

	// Add start directly to the polyline as a straight-line segment
	newPath->controlPoints.push_back(ofVec2f(start.x, start.y));
	newPath->polyline.addVertex(start.x, start.y, 0);
	newPath->attachedPoints.push_back(&start);

	// Track visited; track current position as floats
	std::unordered_set<DataPoint> visited;
	visited.insert(start);
	float cx = start.x, cy = start.y;

	for (int i = 0; i < maxPoints - 1; ++i) {
		// Build list of unvisited candidates with their squared distances
		std::vector<std::pair<float, const DataPoint *>> candidates;
		candidates.reserve(points.size());
		for (const auto & p : points) {
			if (visited.find(p) == visited.end()) {
				float dx = p.x - cx, dy = p.y - cy;
				candidates.push_back({ dx * dx + dy * dy, &p });
			}
		}
		if (candidates.empty()) break;

		// Sort closest first
		std::sort(candidates.begin(), candidates.end(),
			[](const auto & a, const auto & b) { return a.first < b.first; });

		// Choose next with randomness
		int poolSize = std::min(numNeighbors, (int)candidates.size());
		const DataPoint * next;
		if (ofRandom(1.0f) < randomness) {
			int idx = (int)ofRandom((float)poolSize);
			next = candidates[std::min(idx, poolSize - 1)].second;
		} else {
			next = candidates[0].second;
		}

		// Add straight-line vertex to path
		newPath->controlPoints.push_back(ofVec2f(next->x, next->y));
		newPath->polyline.addVertex(next->x, next->y, 0);
		newPath->attachedPoints.push_back(next);
		visited.insert(*next);
		cx = next->x;
		cy = next->y;
	}

	return newPath;
}

//--------------------------------------------------------------
void ofApp::stopPathSamples(std::shared_ptr<PathObject> p) {
	if (!p) return;
	for (const auto & dp : p->playingPoints) {
		oscManager.stopSample(mediaRoot + dp.filename, p->name);
	}
	p->playingPoints.clear();
}

//--------------------------------------------------------------
void ofApp::saveComposition(string filepath) {
	ofxJSONElement root;

	// 1. Data Source
	root["points_file"] = lastLoadedPointsPath;

	// Title
	root["title"] = compositionTitle;

	// 2. Settings / Camera
	root["settings"]["zoom"] = zoom;
	root["settings"]["pan_x"] = pan.x;
	root["settings"]["pan_y"] = pan.y;

	// 3. Paths
	ofxJSONElement pathsArray;
	for (int i = 0; i < paths.size(); ++i) {
		auto & p = paths[i];
		ofxJSONElement pathJson;
		pathJson["name"] = p->name;
		pathJson["is_active"] = p->isActive;
		pathJson["radius"] = p->radius;
		pathJson["speed"] = p->speed;
		pathJson["volume"] = p->volume;
		pathJson["sample_num"] = p->sampleNum;
		pathJson["direction"] = p->direction;
		pathJson["falloff"] = p->falloff;
		pathJson["mode"] = p->mode;
		pathJson["is_sequential"] = p->isSequential;
		pathJson["is_wander"] = p->isWander;
		pathJson["send_video"] = p->sendToVideo;

		// Save control points (the actual drawn/generated vertices)
		ofxJSONElement cpArray;
		for (int j = 0; j < p->controlPoints.size(); ++j) {
			ofxJSONElement cp;
			cp["x"] = p->controlPoints[j].x;
			cp["y"] = p->controlPoints[j].y;
			cpArray.append(cp);
		}
		pathJson["control_points"] = cpArray;

		// Save specific sequential points if applicable
		if (p->isSequential) {
			ofxJSONElement seqArray;
			for (int j = 0; j < p->sequentialPoints.size(); ++j) {
				ofxJSONElement sq;
				sq["x"] = p->sequentialPoints[j].x;
				sq["y"] = p->sequentialPoints[j].y;
				sq["filename"] = p->sequentialPoints[j].filename;
				seqArray.append(sq);
			}
			pathJson["sequential_points"] = seqArray;
		}

		if (!p->attachedPoints.empty()) {
			ofxJSONElement attachedArray;
			for (int j = 0; j < p->attachedPoints.size(); ++j) {
				attachedArray.append(p->attachedPoints[j]->filename);
			}
			pathJson["attached_points"] = attachedArray;
		}

		// Save gesture points if applicable
		if (p->hasGesture && !p->gesturePoints.empty()) {
			pathJson["has_gesture"] = true;
			ofxJSONElement gArray;
			for (int j = 0; j < p->gesturePoints.size(); ++j) {
				ofxJSONElement g;
				g["position"] = p->gesturePoints[j].position;
				g["volume"] = p->gesturePoints[j].volume;
				g["timeMs"] = (Json::Int64)p->gesturePoints[j].timeMs;
				gArray.append(g);
			}
			pathJson["gesture_points"] = gArray;
		} else {
			pathJson["has_gesture"] = false;
		}

		pathsArray.append(pathJson);
	}
	root["paths"] = pathsArray;

	// 4. Embed annotations in composition (self-contained)
	ofxJSONElement annotArray;
	for (int i = 0; i < (int)annotationManager.getAnnotations().size(); ++i) {
		const auto & a = annotationManager.getAnnotations()[i];
		ofxJSONElement obj;
		obj["id"]               = a.id;
		obj["anchor_x"]         = a.anchorPoint.x;
		obj["anchor_y"]         = a.anchorPoint.y;
		obj["nearest_idx"]      = a.nearestPointIdx;
		obj["label_x"]          = a.labelBoxPos.x;
		obj["label_y"]          = a.labelBoxPos.y;
		obj["label_is_world"]   = true;
		obj["label_text"]       = a.labelText;
		obj["is_cluster"]       = a.isClusterAnnotation;
		obj["cluster_id"]       = a.clusterId;
		annotArray.append(obj);
	}
	root["annotations"] = annotArray;

	// Save to disk
	bool success = root.save(filepath, true);
	if (success) {
		ofLogNotice("ofApp::saveComposition") << "Successfully saved composition to " << filepath
			<< " (with " << annotationManager.getAnnotations().size() << " annotations)";
	} else {
		ofLogError("ofApp::saveComposition") << "Failed to save composition to " << filepath;
	}
}

//--------------------------------------------------------------
void ofApp::loadCompositionOrPoints(string filepath) {
	ofxJSONElement root;
	if (!root.open(filepath)) {
		ofLogError("ofApp::loadCompositionOrPoints") << "Failed to open JSON file: " << filepath;
		return;
	}

	// 1. Check if this is a composition by looking if it's an object containing "points_file"
	if (root.isObject() && root.isMember("points_file")) {
		ofLogNotice("ofApp::loadCompositionOrPoints") << "Loading composition from " << filepath;

		// 1. Load the underlying data points
		string targetPointsFile = root["points_file"].asString();

		if (!ofFile(targetPointsFile).exists()) {
			ofSystemAlertDialog("The points file could not be found:\n" + targetPointsFile + "\n\nPlease locate it now to repair the composition.");
			ofFileDialogResult result = ofSystemLoadDialog("Locate " + ofFilePath::getFileName(targetPointsFile));
			if (result.bSuccess) {
				targetPointsFile = result.getPath();
				root["points_file"] = targetPointsFile;
				// Rewrite the composition file with the new path
				root.save(filepath, true);
				ofLogNotice("ofApp::loadCompositionOrPoints") << "Repaired composition with new points file: " << targetPointsFile;
			} else {
				ofLogError("ofApp::loadCompositionOrPoints") << "Repair cancelled. Cannot load composition.";
				return;
			}
		}

		if (!loadPoints(targetPointsFile, false)) {
			// If loadPoints fails (e.g. wrong file type), abort composition load
			ofLogError("ofApp::loadCompositionOrPoints") << "Failed to load points data. Aborting composition load.";
			return;
		}

		// Load title
		if (root.isMember("title")) {
			compositionTitle = root["title"].asString();
			showTitle = true; // Auto-show if loaded? (Can keep it optional, let's auto-show if it exists and is not empty)
			if (compositionTitle.empty()) showTitle = false;
		}

		// 2. Clear existing paths and selection
		paths.clear();
		selectedPath = nullptr;
		oscManager.sendReset(); // Notify SuperCollider of standard reset
		oscManager.sendClear(); // Tell UI/SC to clear visuals/synths

		// Restore path-0 immediately
		auto p0 = std::make_shared<PathObject>(0);
		p0->name = "path-0";
		p0->mode = defaultPathMode;
		p0->isActive = false;
		p0->sendToVideo = false;
		oscManager.sendUIPathAdd(p0->name);
		paths.push_back(p0);

		// 3. Ignore saved pan/zoom and always fit to point-cloud extents on load.

		// 4. Restore paths
		if (root.isMember("paths") && root["paths"].isArray()) {
			const ofxJSONElement & jsonPaths = root["paths"];
			for (int i = 0; i < jsonPaths.size(); ++i) {
				const ofxJSONElement & pJson = jsonPaths[i];

				// Reconstruct PathObject
				auto newPath = std::make_shared<PathObject>(pathIdCounter++);
				newPath->name = pJson["name"].asString();
				newPath->isActive = false; // User requested paths not playing by default on load
				// (We could read but ignore pJson["is_active"].asBool())
				newPath->radius = pJson["radius"].asFloat();
				newPath->speed = pJson["speed"].asFloat();

				// Handle legacy missing properties gracefully
				if (pJson.isMember("volume")) newPath->volume = pJson["volume"].asFloat();
				if (pJson.isMember("sample_num")) newPath->sampleNum = pJson["sample_num"].asInt();
				if (pJson.isMember("direction")) newPath->direction = pJson["direction"].asInt();
				if (pJson.isMember("falloff")) newPath->falloff = pJson["falloff"].asFloat();
				if (pJson.isMember("mode")) newPath->mode = pJson["mode"].asInt();
				if (pJson.isMember("is_sequential")) newPath->isSequential = pJson["is_sequential"].asBool();
				if (pJson.isMember("is_wander")) newPath->isWander = pJson["is_wander"].asBool();
				if (pJson.isMember("send_video")) newPath->sendToVideo = pJson["send_video"].asBool();

				// Reconstruct control points
				if (pJson.isMember("control_points") && pJson["control_points"].isArray()) {
					const ofxJSONElement & cpArray = pJson["control_points"];
					for (int j = 0; j < cpArray.size(); ++j) {
						ofVec2f pt(cpArray[j]["x"].asFloat(), cpArray[j]["y"].asFloat());
						newPath->addPoint(pt);
					}
					if (!newPath->isSequential && !newPath->isWander) { // Normal drawn paths need finalization (smoothing)
						newPath->finalize();
					}
				}

				// Reconstruct sequential points (if applicable)
				if (newPath->isSequential && pJson.isMember("sequential_points") && pJson["sequential_points"].isArray()) {
					const ofxJSONElement & seqArray = pJson["sequential_points"];
					for (int j = 0; j < seqArray.size(); ++j) {
						DataPoint dp;
						dp.x = seqArray[j]["x"].asFloat();
						dp.y = seqArray[j]["y"].asFloat();
						dp.filename = seqArray[j]["filename"].asString();
						newPath->sequentialPoints.push_back(dp);
					}
				}

				if (pJson.isMember("attached_points") && pJson["attached_points"].isArray()) {
					const ofxJSONElement & attArray = pJson["attached_points"];
					for (int j = 0; j < attArray.size(); ++j) {
						std::string fn = attArray[j].asString();
						for (const auto & dp : points) {
							if (dp.filename == fn) {
								newPath->attachedPoints.push_back(&dp);
								break;
							}
						}
					}
				}

				// Reconstruct gesture points (if applicable)
				if (pJson.isMember("has_gesture") && pJson["has_gesture"].asBool()) {
					if (pJson.isMember("gesture_points") && pJson["gesture_points"].isArray()) {
						newPath->hasGesture = true;
						const ofxJSONElement & gArray = pJson["gesture_points"];
						for (int j = 0; j < gArray.size(); ++j) {
							PathObject::GesturePoint gp;
							gp.position = gArray[j]["position"].asFloat();
							gp.volume = gArray[j]["volume"].asFloat();
							// Default timeMs to 0 for older un-versioned saves to avoid crash, but they won't playback dynamically
							gp.timeMs = gArray[j].isMember("timeMs") ? gArray[j]["timeMs"].asInt64() : 0;
							newPath->gesturePoints.push_back(gp);
						}
					}
				}

				paths.push_back(newPath);
				oscManager.sendUIPathAdd(newPath->name);
				sendFullUIUpdate(newPath);
			}
		}

		// 5. Load annotations from composition JSON (not from global file)
		if (root.isMember("annotations") && root["annotations"].isArray()) {
			annotationManager.loadFromJSON(root["annotations"], points);
			ofLogNotice("ofApp::loadCompositionOrPoints") << "Loaded " 
				<< annotationManager.getAnnotations().size() << " annotations from composition.";
		} else {
			// No annotations in composition; start with empty
			annotationManager.clearAnnotations();
		}

		// Register cluster annotations for any labelled clusters
		for (const auto & kv : clusters) {
			if (!kv.second.label.empty()) {
				annotationManager.addClusterAnnotation(kv.second.id, kv.second.label, points);
			}
		}

		// Auto-fit loaded composition to point extents immediately.
		zoomToDataExtents(false, false);

		ofLogNotice("ofApp::loadCompositionOrPoints") << "Composition loaded successfully!";
	} else {
		// 2. Otherwise, treat it as a standard raw points file
		ofLogNotice("ofApp::loadCompositionOrPoints") << "No composition metadata found; treating as raw points file.";
		loadPoints(filepath, true);
	}
}