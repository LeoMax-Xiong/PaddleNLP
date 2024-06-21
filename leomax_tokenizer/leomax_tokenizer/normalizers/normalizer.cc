#include <iostream>
#include "normalizer.h"
#include <codecvt>
#include <locale>
#include <cstring>
#include "glog/logging.h"
#include "../utils/utf8.h"
namespace leomax_tokenizer {
namespace normalizers {
NormalizedString::NormalizedString(const std::string& original) :
                original_(original),
                normalized_(original),
                original_shift_(0) { 
    std::cout << "==================== Normalized String Constructor ===============" << std::endl;
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> conv;
    std::u32string u32normalized = conv.from_bytes(normalized_);
    std::cout << "u32normalized: " << u32normalized.length() << std::endl;
    for (int i = 0; i < u32normalized.length(); ++i) {
        auto new_normalized_char_len = utils::get_utf8_char_len(u32normalized[i]);
        uint32_t start = 0;
        uint32_t end = 0;
        if (i != 0) {
            start = alignments_.back().second;
        }
        end = start + new_normalized_char_len;
        std::cout << "u32 normalized start: " << start << ", end: " << end << std::endl;
        for (int j = 0; j < new_normalized_char_len; ++j) {
            alignments_.push_back({start, end});
        }
    }

    std::cout << "alignments: " << alignments_.size() << std::endl;
    for (auto itr = alignments_.begin(); itr != alignments_.end(); ++itr){
        std::cout << "alignments: " << itr->first << ", " << itr->second << std::endl;
    }
    
}
NormalizedString::NormalizedString(NormalizedString&& other):
    original_(std::move(other.original_)),
    normalized_(std::move(other.normalized_)),
    alignments_(std::move(other.alignments_)),
    original_shift_(other.original_shift_) {

}

NormalizedString&  NormalizedString::operator=(NormalizedString&& other) {
    original_ = std::move(other.original_);
    normalized_ = std::move(other.normalized_);
    alignments_ = std::move(other.alignments_);
    original_shift_ = other.original_shift_;
    return *this;
}

bool NormalizedString::slice(core::Range range, NormalizedString* normalized, bool origin_range) const {
    std::cout << "normalized string slice " << std::endl;
    core::Range normalized_range = range;
    core::Range original_range = range;
    std::cout << "slice origin range: " << origin_range << std::endl;
    if (origin_range){ 
        convert_offsets(&normalized_range, true);
    } else {
        convert_offsets(&original_range, false);
    }

    uint32_t n_shift = original_range.first;
    // 限制 original_range.first 不能超过原始长度
    original_range.first =
        (std::min)(original_range.first,
                   static_cast<uint32_t>(this->original_.length() - 1));
    std::string old_original = normalized->original_;
    normalized->original_ = this->original_.substr(
                    original_range.first, original_range.second - original_range.first);
    VLOG(6) << "updated the normalized string from: [ " 
             << old_original << " ] to : [ " 
             << normalized->original_ << " ]";

    // 重新设置 normalized_range 的first
    normalized_range.first =
        (std::min)(normalized_range.first,
                   static_cast<uint32_t>(this->normalized_.length() - 1));

    normalized->normalized_ = this->normalized_.substr(
                                    normalized_range.first,
                                    normalized_range.second - normalized_range.first);
    VLOG(6) << "updated the normalized string range second: " << normalized_range.second;
    normalized->alignments_.reserve(normalized_range.second -
                                    normalized_range.first);

    for (uint32_t i = normalized_range.first; i < normalized_range.second;
         ++i) {
        normalized->alignments_.emplace_back(
                        this->alignments_[i].first - n_shift,
                        this->alignments_[i].second - n_shift);
    }
    VLOG(6) << "update the alignments size: " << normalized->alignments_.size();
    normalized->original_shift_ = this->original_shift_ + original_range.first;
    VLOG(6) << "update the original shift: " << normalized->original_shift_;
    return true;

}

bool NormalizedString::convert_offsets(core::Range* range, 
                                       bool origin_range) const {
    std::cout << "convert offsets" << std::endl;
    auto len_original = get_original_len();
    auto len_normalized = get_len();
    std::cout << "len original: " << len_original << std::endl;
    std::cout << "len normalized: " << len_normalized << std::endl;
    std::cout << "range: [ " << range->first << ", " << range->second << " ]" << std::endl;
    VLOG(6) << "origin range: " << origin_range;
    if (range->first == range->second) {
        return true;
    }

    if (range->first > range->second) {
        return false;
    }

    if (origin_range && original_.empty() && 
        (range->first == 0 && range->second == 0)) {
        range->second = len_normalized;
        return true;
    }

    if (!origin_range && normalized_.empty() && 
        (range->first == 0 && range->second == 0)) {
        range->second = len_original;
        return true;
    }

    if (origin_range) {
        int start = -1;
        int end = -1;
        for (int i = 0; i < alignments_.size(); ++i) {
            if (range->second >= alignments_[i].second) {
                if (start < 0 && range->first <= alignments_[i].first) {
                    if (alignments_[i].first != alignments_[i].second) {
                        start = i;
                    }
                }
                if (range->second >= alignments_[i].second) {
                    end = i + 1;
                }
            }
        }
        if (start > 0 && end < 0) {
            *range = {start, start};
        } else if (start < 0 && end > 0) {
            *range = {end, end};
        } else if (start > 0 && end > 0) {
            *range = {start, end};
        } else {
            return false;
        }
    } else {
        std::cout << "convert offsets alignments size: " << alignments_.size() << std::endl;
        range->first = alignments_[range->first].first;
        range->second = alignments_[range->second - 1].second; // [start, end)
    }

    std::cout << "convert offsets finished" << std::endl;
    return true;
}

NormalizedString& NormalizedString::filter_char(std::function<bool(char32_t)> keep_char_fn) {
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> conv;
    std::u32string u32new_normalized;
    u32new_normalized.reserve(this->normalized_.length());
    uint32_t removed_start;
    uint32_t removed = 0;
    std::vector<int> changes;

    changes.reserve(normalized_.length());
    bool has_init_ch = false;
    uint32_t last_char;
    uint32_t curr_char;

    size_t utf8_len = 0;
    while (utf8_len < normalized_.length()) {
        auto chwidth =
            utils::utf8_to_uint32(normalized_.data() + utf8_len, &curr_char);
        curr_char = utils::utf8_to_unicode(curr_char);
        if (keep_char_fn(curr_char)) {
            if (has_init_ch) {
                u32new_normalized.push_back(last_char);
                changes.push_back(-removed);
            } else {
                has_init_ch = true;
                removed_start = removed;
            }
            last_char = curr_char;
            removed = 0;
        } else {
            removed += 1; 
        }
        utf8_len += chwidth;
    }

    if (has_init_ch) {
        u32new_normalized.push_back(last_char);
        changes.push_back(-removed);
    }
    OffsetMapping new_normalized_offset{u32new_normalized, changes};
    // Update normalized_ and alignments_
    // UpdateNormalized(new_normalized_offset, removed_start);
    update_normalized(new_normalized_offset, removed_start);
    return *this;

}

void NormalizedString::update_normalized_range(const OffsetMapping& new_normalized,
                            uint32_t initial_offset, core::Range range,
                            bool origin_range) {
    auto n_range = range;
    if (origin_range) {
        convert_offsets(&n_range, origin_range);
    }

    // Retrieve the original characters that are being replaced. This let us
    // compute the change in byte sizes along the way.
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> conv;
    n_range.first = (std::min)(n_range.first,
                                static_cast<uint32_t>(normalized_.length() - 1));

    std::u32string u32replaced_normalized = conv.from_bytes(
        normalized_.substr(n_range.first, n_range.second - n_range.first));
    uint32_t initial_removed = 0;

    // calculate initial_removed
    for (int i = 0; i < initial_offset; ++i) {
        size_t chwidth = utils::get_utf8_char_len(u32replaced_normalized[i]);
        initial_removed += chwidth;
    }

    uint32_t offset = initial_removed + n_range.first;
    std::vector<core::Range> alignments;
    alignments.reserve(n_range.second - n_range.first);

    int replaced_normalized_idx = initial_removed;
    // Calculate the new alignments
    for (int i = 0; i < new_normalized.u32normalized.length(); ++i) {
        auto idx = offset;
        core::Range align;
        int curr_changes = new_normalized.changes[i];
        if (curr_changes > 0) {
            // Insert a char
            if (idx < 1) {
                align = {0, 0};
            } else {
                align = alignments_[idx - 1];
            }
        } else {
            align = alignments_[idx];
        }
        char32_t new_normalized_char = new_normalized.u32normalized[i];
        auto new_normalized_char_len = utils::get_utf8_char_len(new_normalized_char);
        char32_t replaced_char = -1;
        if (curr_changes <= 0) {
            replaced_char = u32replaced_normalized[replaced_normalized_idx++];
        }
        uint32_t replaced_char_size =
            (replaced_char == -1) ? 0 : utils::get_utf8_char_len(replaced_char);

        uint32_t total_bytes_to_remove = 0;
        if (curr_changes < 0) {
            for (int j = 0; j < -curr_changes; ++j) {
                replaced_char = u32replaced_normalized[replaced_normalized_idx++];
                total_bytes_to_remove += utils::get_utf8_char_len(replaced_char);
            }
        }
        offset += replaced_char_size + total_bytes_to_remove;
        alignments.insert(alignments.end(), new_normalized_char_len, align);
    }
    // Replace the old alignments in n_range
    if (n_range.second - n_range.first >= alignments.size()) {
        std::memcpy(alignments_.data() + n_range.first,
                    alignments.data(),
                    alignments.size() * sizeof(core::Range));
        alignments_.erase(alignments_.begin() + n_range.first + alignments.size(),
                        alignments_.begin() + n_range.second);
    } else {
        std::vector<core::Range> new_alignments;
        auto third_len = 0;
        if (alignments_.size() > n_range.second) {
            third_len = alignments_.size() - n_range.second;
        }
        new_alignments.resize(n_range.first + alignments.size() + third_len);
        if (n_range.first > 0) {
            std::copy_n(alignments_.begin(), n_range.first, new_alignments.begin());
        }
        std::copy_n(alignments.begin(),
                    alignments.size(),
                    new_alignments.begin() + n_range.first);
        if (third_len > 0) {
            std::copy_n(alignments_.begin() + n_range.second,
                    third_len,
                    new_alignments.begin() + n_range.first + alignments.size());
        }
        alignments_ = std::move(new_alignments);
    }
    // Unicode -> UTF8
    uint32_t normalized_utf8_size = 0;
    for (auto& ch : new_normalized.u32normalized) {
        normalized_utf8_size += utils::get_utf8_char_len(ch);
    }
    std::vector<char> utf8_str(normalized_utf8_size + 1);
    utils::get_utf8_str(new_normalized.u32normalized.data(),
                        utf8_str.data(),
                        new_normalized.u32normalized.length());

    // Update normalized_
    auto normalized_iter = normalized_.begin();
    normalized_.replace(normalized_iter + n_range.first,
                        normalized_iter + n_range.second,
                        utf8_str.data(),
                        normalized_utf8_size);
    
} 

}
}