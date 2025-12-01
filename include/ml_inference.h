#ifndef ML_INFERENCE_H
#define ML_INFERENCE_H

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdexcept>

// Forward declarations for fastText and LightGBM if we decide to use their C++ APIs directly
// Otherwise, we might wrap them or use a C-API if available.
// For now, let's assume we'll use their C++ headers.
#include "fasttext.h" // Assuming fastText C++ header path
#include "LightGBM/boosting.h" // Assuming LightGBM C++ header path
#include "LightGBM/objective_function.h"
#include "LightGBM/config.h"
#include "LightGBM/dataset.h"
#include "LightGBM/metadata.h"


// Simple JSON parser for meta.json (can be replaced by a proper library like nlohmann/json)
namespace json_parser {
    std::map<std::string, std::vector<std::string>> parse_meta(const std::string& filepath) {
        std::map<std::string, std::vector<std::string>> meta_data;
        std::ifstream ifs(filepath);
        if (!ifs.is_open()) {
            throw std::runtime_error("Could not open meta.json file: " + filepath);
        }
        std::string line;
        std::string content;
        while (std::getline(ifs, line)) {
            content += line;
        }

        // Basic parsing for specific keys. This is very fragile and should be replaced by a proper JSON library.
        // For example, to parse "classes": ["Game", "Browser", "Other"]
        size_t pos = content.find("\"classes\":");
        if (pos != std::string::npos) {
            size_t start = content.find("[", pos);
            size_t end = content.find("]", start);
            std::string classes_str = content.substr(start + 1, end - start - 1);
            std::stringstream ss(classes_str);
            std::string segment;
            while(std::getline(ss, segment, ',')) {
                segment.erase(0, segment.find_first_not_of(" \t\n\r\""));
                segment.erase(segment.find_last_not_of(" \t\n\r\"") + 1);
                if (!segment.empty()) {
                    meta_data["classes"].push_back(segment);
                }
            }
        }

        pos = content.find("\"text_cols\":");
        if (pos != std::string::npos) {
            size_t start = content.find("[", pos);
            size_t end = content.find("]", start);
            std::string cols_str = content.substr(start + 1, end - start - 1);
            std::stringstream ss(cols_str);
            std::string segment;
            while(std::getline(ss, segment, ',')) {
                segment.erase(0, segment.find_first_not_of(" \t\n\r\""));
                segment.erase(segment.find_last_not_of(" \t\n\r\"") + 1);
                if (!segment.empty()) {
                    meta_data["text_cols"].push_back(segment);
                }
            }
        }

        pos = content.find("\"numeric_cols\":");
        if (pos != std::string::npos) {
            size_t start = content.find("[", pos);
            size_t end = content.find("]", start);
            std::string cols_str = content.substr(start + 1, end - start - 1);
            std::stringstream ss(cols_str);
            std::string segment;
            while(std::getline(ss, segment, ',')) {
                segment.erase(0, segment.find_first_not_of(" \t\n\r\""));
                segment.erase(segment.find_last_not_of(" \t\n\r\"") + 1);
                if (!segment.empty()) {
                    meta_data["numeric_cols"].push_back(segment);
                }
            }
        }
        return meta_data;
    }
}


class MLInference {
public:
    MLInference(const std::string& ft_model_path, const std::string& lgbm_model_path, const std::string& meta_path);
    ~MLInference();

    std::string predict(const std::map<std::string, std::string>& raw_data);

private:
    fasttext::FastText ft_model_;
    std::unique_ptr<LightGBM::Boosting> lgbm_booster_;

    std::vector<std::string> classes_;
    std::vector<std::string> text_cols_;
    std::vector<std::string> numeric_cols_;
    int embedding_dim_;

    std::string normalize_text(const std::string& text);
    std::vector<double> get_feature_vector(const std::map<std::string, std::string>& raw_data);

public: // Public getters for feature lists
    const std::vector<std::string>& getTextCols() const { return text_cols_; }
    const std::vector<std::string>& getNumericCols() const { return numeric_cols_; }
};

#endif // ML_INFERENCE_H
