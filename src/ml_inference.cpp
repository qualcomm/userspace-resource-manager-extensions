#include "ml_inference.h"
#include <iostream>
#include <sstream>
#include <fstream> // Add this include for std::ifstream
#include <algorithm> // Add this include for std::transform
#include <stdexcept> // Add this include for std::runtime_error

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

// Placeholder for actual fastText and LightGBM implementation
// This is a minimal implementation to satisfy the compilation for now.

// Helper to convert std::string to float, handling potential errors
float string_to_float(const std::string& s) {
    try {
        return std::stof(s);
    } catch (const std::invalid_argument& e) {
        return 0.0f; // Default to 0.0 or NaN, depending on desired behavior
    } catch (const std::out_of_range& e) {
        return 0.0f; // Default to 0.0 or NaN
    }
}

MLInference::MLInference(const std::string& ft_model_path, const std::string& lgbm_model_path, const std::string& meta_path) {
    std::cout << "Parsing meta.json from: " << meta_path << std::endl;
    auto meta_data = json_parser::parse_meta(meta_path);
    classes_ = meta_data["classes"];
    text_cols_ = meta_data["text_cols"];
    numeric_cols_ = meta_data["numeric_cols"];

    std::cout << "Loading fastText model from: " << ft_model_path << std::endl;
    ft_model_.loadModel(ft_model_path); // Assuming fastText C++ API has loadModel method
    embedding_dim_ = ft_model_.getDimension();
    std::cout << "fastText embedding dimension: " << embedding_dim_ << std::endl;

    std::cout << "Loading LightGBM model from: " << lgbm_model_path << std::endl;
    // Use the CreateBoosting overload that takes booster_type and filename
    lgbm_booster_ = std::unique_ptr<LightGBM::Boosting>(
        LightGBM::Boosting::CreateBoosting("gbdt", lgbm_model_path.c_str())
    );
}

MLInference::~MLInference() {
    // Destructor for MLInference
}

std::string MLInference::normalize_text(const std::string& text) {
    std::string s = text;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    // Add other normalization steps as needed (e.g., replace newlines, remove special chars)
    // For now, mirroring _norm_text from Python:
    // s = s.replace('\n', ' ').replace('\t', ' ')
    // s = re.sub(r'[^A-Za-z0-9_:\-]+', ' ', s)
    return s;
}

std::vector<double> MLInference::get_feature_vector(const std::map<std::string, std::string>& raw_data) {
    std::vector<double> feature_vector;

    // 1. Numeric features
    for (const auto& col : numeric_cols_) {
        auto it = raw_data.find(col);
        if (it != raw_data.end()) {
            feature_vector.push_back(string_to_float(it->second));
        } else {
            feature_vector.push_back(0.0); // Fill missing numeric with 0.0 as per notebook
        }
    }

    // 2. fastText embeddings
    std::string concatenated_text;
    for (const auto& col : text_cols_) {
        auto it = raw_data.find(col);
        if (it != raw_data.end()) {
            concatenated_text += normalize_text(it->second) + " ";
        } else {
            concatenated_text += " "; // Add space for missing text columns
        }
    }
    // Remove trailing space if any
    if (!concatenated_text.empty() && concatenated_text.back() == ' ') {
        concatenated_text.pop_back();
    }
    
    if (!concatenated_text.empty()) {
        fasttext::Vector ft_embedding_vector(embedding_dim_);
        std::istringstream iss(concatenated_text);
        ft_model_.getSentenceVector(iss, ft_embedding_vector);
        for (int i = 0; i < embedding_dim_; ++i) {
            feature_vector.push_back(static_cast<double>(ft_embedding_vector[i]));
        }
    } else {
        // If no text, fill with zeros for embedding
        for (int i = 0; i < embedding_dim_; ++i) {
            feature_vector.push_back(0.0);
        }
    }

    return feature_vector;
}

std::string MLInference::predict(const std::map<std::string, std::string>& raw_data) {
    std::vector<double> features = get_feature_vector(raw_data);
    
    // LightGBM prediction
    std::vector<double> result(classes_.size());
    lgbm_booster_->Predict(features.data(), &result[0], nullptr);

    // Get the predicted class index
    int predicted_class_idx = 0;
    double max_prob = -1.0;
    for (size_t i = 0; i < classes_.size(); ++i) {
        if (result[i] > max_prob) {
            max_prob = result[i];
            predicted_class_idx = i;
        }
    }

    return classes_[predicted_class_idx];
}
