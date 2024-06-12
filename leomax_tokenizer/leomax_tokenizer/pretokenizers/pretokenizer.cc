#include "pretokenizer.h"

namespace leomax_tokenizer {
namespace pretokenizers {

PreTokenizedString::PreTokenizedString(const std::string& original) 
    : original_(original) {
    this->splits_.emplace_back(std::move(StringSplit(original_)));
}

void PreTokenizedString::set_original_str(const std::string& orignal) {
    this->original_ = orignal;
    this->splits_.clear();
    this->splits_.emplace_back(this->original_);
}

void PreTokenizedString::split(std::function<void(int idx, 
                                                  normalizers::NormalizedString*,
                                                  std::vector<StringSplit>*)> split_fn) {
    // 遍历 splits_，如果 tokens 的数据 > 0，那么将该token保存到新的split中
    // todo: split_ 怎么获取？？
    std::vector<StringSplit> new_splits;        // 存储新的分词结果
    new_splits.reserve(this->splits_.size());

    for (int i = 0; i < this->splits_.size(); ++i) {
        // 如果 tokens 的数据 > 0，那么将该token保存到新的split中，不会重复分词
        if (this->splits_[i].tokens_.size() > 0) {     
            new_splits.push_back(std::move(this->splits_[i]));
            continue;
        }

        // 否则，调用 split_fn 进行分词
        split_fn(i, &this->splits_[i].normalized_, &new_splits);
    }

    this->splits_ = std::move(new_splits);
}

}       // namespace pretokenizers

}       // namepsace leomax_tokenizer