#include "NameGen.hpp"

#include <Utils/Filesystem/FileReader.hpp>

#include <algorithm>
#include <cctype>
#include <utility>

namespace dotnamebot::namegen {

  NameGen::NameGen(std::shared_ptr<dotnamebot::logging::ILogger> logger,
                   std::shared_ptr<dotnamebot::assets::IAssetManager> assetManager)
      : logger_(std::move(logger)), assetManager_(std::move(assetManager)),
        rng_(std::random_device{}()) {
    initialize();
  }

  bool NameGen::initialize() {
    const auto adjPath = assetManager_->getAssetsPath() / "adjectives.txt";
    const auto nounPath = assetManager_->getAssetsPath() / "nouns.txt";

    dotnamebot::utils::FileReader reader;

    auto adjResult = reader.readLines(adjPath);
    if (!adjResult) {
      logger_->error("NameGen: failed to load adjectives.txt: " + adjResult.error().toString());
      return false;
    }
    adjectives_ = parseWordList(adjResult.value());

    auto nounResult = reader.readLines(nounPath);
    if (!nounResult) {
      logger_->error("NameGen: failed to load nouns.txt: " + nounResult.error().toString());
      return false;
    }
    nouns_ = parseWordList(nounResult.value());

    logger_->infoStream() << "NameGen: loaded " << adjectives_.size() << " adjectives, "
                          << nouns_.size() << " nouns";
    return true;
  }

  std::string NameGen::generate() const {
    if (adjectives_.empty() || nouns_.empty()) {
      return {};
    }

    std::uniform_int_distribution<std::size_t> adjDist(0, adjectives_.size() - 1);
    std::uniform_int_distribution<std::size_t> nounDist(0, nouns_.size() - 1);

    return capitalize(adjectives_[adjDist(rng_)]) + " " + capitalize(nouns_[nounDist(rng_)]);
  }

  std::string NameGen::capitalize(const std::string &word) {
    if (word.empty()) {
      return word;
    }
    std::string result = word;
    result[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(result[0])));
    return result;
  }

  std::vector<std::string> NameGen::parseWordList(const std::vector<std::string> &lines) {
    std::vector<std::string> words;
    words.reserve(lines.size());
    for (const auto &line : lines) {
      // Trim whitespace
      std::string w = line;
      w.erase(w.begin(),
              std::find_if(w.begin(), w.end(), [](unsigned char ch) { return !std::isspace(ch); }));
      w.erase(std::find_if(w.rbegin(), w.rend(),
                           [](unsigned char ch) {
        return !std::isspace(ch);
      }).base(),
              w.end());
      if (!w.empty()) {
        words.push_back(std::move(w));
      }
    }
    return words;
  }

} // namespace dotnamebot::namegen
