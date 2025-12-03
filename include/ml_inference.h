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
//#include "LightGBM/metadata.h"
#include "LightGBM/meta.h"


namespace json_parser {
    std::map<std::string, std::vector<std::string>> parse_meta(const std::string& filepath);
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
