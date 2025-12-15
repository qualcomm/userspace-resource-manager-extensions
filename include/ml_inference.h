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

#include <mutex>


class MLInference {
public:
    MLInference(const std::string& ft_model_path);
    ~MLInference();

    std::string predict(int pid, const std::map<std::string, std::string>& raw_data);

private:
    fasttext::FastText ft_model_;
    std::mutex predict_mutex_;

    std::vector<std::string> classes_;
    std::vector<std::string> text_cols_;
    int embedding_dim_;

    std::string normalize_text(const std::string& text);

public: // Public getters for feature lists
    const std::vector<std::string>& getTextCols() const { return text_cols_; }
};

#endif // ML_INFERENCE_H
