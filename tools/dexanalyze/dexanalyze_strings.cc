/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "dexanalyze_strings.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <queue>

#include "dex/class_accessor-inl.h"
#include "dex/code_item_accessors-inl.h"
#include "dex/dex_instruction-inl.h"

namespace art {
namespace dexanalyze {

// Tunable parameters.
static const size_t kMinPrefixLen = 1;
static const size_t kMaxPrefixLen = 255;
static const size_t kPrefixConstantCost = 4;
static const size_t kPrefixIndexCost = 2;

// Node value = (distance from root) * (occurrences - 1).
class MatchTrie {
 public:
  void Add(const std::string& str) {
    MatchTrie* node = this;
    size_t depth = 0u;
    for (uint8_t c : str) {
      ++depth;
      if (node->nodes_[c] == nullptr) {
        MatchTrie* new_node = new MatchTrie();
        node->nodes_[c].reset(new_node);
        new_node->parent_ = node;
        new_node->depth_ = depth;
        new_node->incoming_ = c;
        node = new_node;
      } else {
        node = node->nodes_[c].get();
      }
      ++node->count_;
    }
    node->is_end_ = true;
  }

  // Returns the length of the longest prefix and if it's a leaf node.
  std::pair<size_t, bool> LongestPrefix(const std::string& str) const {
    const MatchTrie* node = this;
    const MatchTrie* best_node = this;
    size_t depth = 0u;
    size_t best_depth = 0u;
    for (uint8_t c : str) {
      if (node->nodes_[c] == nullptr) {
        break;
      }
      node = node->nodes_[c].get();
      ++depth;
      if (node->is_end_) {
        best_depth = depth;
        best_node = node;
      }
    }
    bool is_leaf = true;
    for (const std::unique_ptr<MatchTrie>& cur_node : best_node->nodes_) {
      if (cur_node != nullptr) {
        is_leaf = false;
      }
    }
    return {best_depth, is_leaf};
  }

  int32_t Savings() const {
    int32_t cost = kPrefixConstantCost;
    int32_t first_used = 0u;
    if (chosen_suffix_count_ == 0u) {
      cost += depth_;
    }
    uint32_t extra_savings = 0u;
    for (MatchTrie* cur = parent_; cur != nullptr; cur = cur->parent_) {
      if (cur->chosen_) {
        first_used = cur->depth_;
        if (cur->chosen_suffix_count_ == 0u) {
          // First suffix for the chosen parent, remove the cost of the dictionary entry.
          extra_savings += first_used;
        }
        break;
      }
    }
    return count_ * (depth_ - first_used) - cost + extra_savings;
  }

  template <typename T, typename... Args, template <typename...> class Queue>
  T PopRealTop(Queue<T, Args...>& queue) {
    auto pair = queue.top();
    queue.pop();
    // Keep updating values until one sticks.
    while (pair.second->Savings() != pair.first) {
      pair.first = pair.second->Savings();
      queue.push(pair);
      pair = queue.top();
      queue.pop();
    }
    return pair;
  }

  std::vector<std::string> ExtractPrefixes(size_t max) {
    std::vector<std::string> ret;
    // Make priority queue and adaptively update it. Each node value is the savings from picking
    // it. Insert all of the interesting nodes in the queue (children != 1).
    std::priority_queue<std::pair<int32_t, MatchTrie*>> queue;
    // Add all of the nodes to the queue.
    std::vector<MatchTrie*> work(1, this);
    while (!work.empty()) {
      MatchTrie* elem = work.back();
      work.pop_back();
      size_t num_childs = 0u;
      for (const std::unique_ptr<MatchTrie>& child : elem->nodes_) {
        if (child != nullptr) {
          work.push_back(child.get());
          ++num_childs;
        }
      }
      if (num_childs > 1u || elem->is_end_) {
        queue.emplace(elem->Savings(), elem);
      }
    }
    std::priority_queue<std::pair<int32_t, MatchTrie*>> prefixes;
    // The savings can only ever go down for a given node, never up.
    while (max != 0u && !queue.empty()) {
      std::pair<int32_t, MatchTrie*> pair = PopRealTop(queue);
      if (pair.second != this && pair.first > 0) {
        // Pick this node.
        uint32_t count = pair.second->count_;
        pair.second->chosen_ = true;
        for (MatchTrie* cur = pair.second->parent_; cur != this; cur = cur->parent_) {
          if (cur->chosen_) {
            break;
          }
          cur->count_ -= count;
        }
        for (MatchTrie* cur = pair.second->parent_; cur != this; cur = cur->parent_) {
          ++cur->chosen_suffix_count_;
        }
        prefixes.emplace(pair.first, pair.second);
        --max;
      } else {
        // Negative or no EV, just delete the node.
      }
    }
    while (!prefixes.empty()) {
      std::pair<int32_t, MatchTrie*> pair = PopRealTop(prefixes);
      if (pair.first <= 0) {
        continue;
      }
      std::vector<uint8_t> chars;
      for (MatchTrie* cur = pair.second; cur != this; cur = cur->parent_) {
        chars.push_back(cur->incoming_);
      }
      ret.push_back(std::string(chars.rbegin(), chars.rend()));
      // LOG(INFO) << pair.second->Savings() << " : " << ret.back();
    }
    return ret;
  }

  std::unique_ptr<MatchTrie> nodes_[256];
  MatchTrie* parent_ = nullptr;
  uint32_t count_ = 0u;
  int32_t depth_ = 0u;
  int32_t savings_ = 0u;
  uint8_t incoming_ = 0u;
  // If the current node is the end of a possible prefix.
  bool is_end_ = false;
  // If the current node is chosen to be a used prefix.
  bool chosen_ = false;
  // If the current node is a prefix of a longer chosen prefix.
  uint32_t chosen_suffix_count_ = 0u;
};

void AnalyzeStrings::ProcessDexFiles(const std::vector<std::unique_ptr<const DexFile>>& dex_files) {
  std::set<std::string> unique_strings;
  // Accumulate the strings.
  for (const std::unique_ptr<const DexFile>& dex_file : dex_files) {
    for (size_t i = 0; i < dex_file->NumStringIds(); ++i) {
      uint32_t length = 0;
      const char* data = dex_file->StringDataAndUtf16LengthByIdx(dex::StringIndex(i), &length);
      // Analyze if the string has any UTF16 chars.
      bool have_wide_char = false;
      const char* ptr = data;
      for (size_t j = 0; j < length; ++j) {
        have_wide_char = have_wide_char || GetUtf16FromUtf8(&ptr) >= 0x100;
      }
      if (have_wide_char) {
        wide_string_bytes_ += 2 * length;
      } else {
        ascii_string_bytes_ += length;
      }
      string_data_bytes_ += ptr - data;
      unique_strings.insert(data);
    }
  }
  // Unique strings only since we want to exclude savings from multidex duplication.
  ProcessStrings(std::vector<std::string>(unique_strings.begin(), unique_strings.end()), 1);
}

void AnalyzeStrings::ProcessStrings(const std::vector<std::string>& strings, size_t iterations) {
  if (iterations == 0u) {
    return;
  }
  // Calculate total shared prefix.
  std::vector<size_t> shared_len;
  prefixes_.clear();
  std::unique_ptr<MatchTrie> prefix_construct(new MatchTrie());
  for (size_t i = 0; i < strings.size(); ++i) {
    size_t best_len = 0;
    if (i > 0) {
      best_len = std::max(best_len, PrefixLen(strings[i], strings[i - 1]));
    }
    if (i < strings.size() - 1) {
      best_len = std::max(best_len, PrefixLen(strings[i], strings[i + 1]));
    }
    best_len = std::min(best_len, kMaxPrefixLen);
    std::string prefix;
    if (best_len >= kMinPrefixLen) {
      prefix = strings[i].substr(0, best_len);
      prefix_construct->Add(prefix);
      ++prefixes_[prefix];
      total_shared_prefix_bytes_ += best_len;
    }
    total_prefix_index_cost_ += kPrefixIndexCost;
  }

  static constexpr size_t kPrefixBits = 15;
  static constexpr size_t kShortLen = (1u << (15 - kPrefixBits)) - 1;
  std::unique_ptr<MatchTrie> prefix_trie(new MatchTrie());
  static constexpr bool kUseGreedyTrie = true;
  if (kUseGreedyTrie) {
    std::vector<std::string> prefixes(prefix_construct->ExtractPrefixes(1 << kPrefixBits));
    for (auto&& str : prefixes) {
      prefix_trie->Add(str);
    }
  } else {
    // Optimize the result by moving long prefixes to shorter ones if it causes additional savings.
    while (true) {
      bool have_savings = false;
      auto it = prefixes_.begin();
      std::vector<std::string> longest;
      for (const auto& pair : prefixes_) {
        longest.push_back(pair.first);
      }
      std::sort(longest.begin(), longest.end(), [](const std::string& a, const std::string& b) {
        return a.length() > b.length();
      });
      // Do longest first since this provides the best results.
      for (const std::string& s : longest) {
        it = prefixes_.find(s);
        CHECK(it != prefixes_.end());
        const std::string& prefix = it->first;
        int64_t best_savings = 0u;
        int64_t best_len = -1;
        for (int64_t len = prefix.length() - 1; len >= 0; --len) {
          auto found = prefixes_.find(prefix.substr(0, len));
          if (len != 0 && found == prefixes_.end()) {
            continue;
          }
          // Calculate savings from downgrading the prefix.
          int64_t savings = kPrefixConstantCost + prefix.length() -
              (prefix.length() - len) * it->second;
          if (savings > best_savings) {
            best_savings = savings;
            best_len = len;
            break;
          }
        }
        if (best_len != -1) {
          prefixes_[prefix.substr(0, best_len)] += it->second;
          it = prefixes_.erase(it);
          optimization_savings_ += best_savings;
          have_savings = true;
        } else {
          ++it;
        }
      }
      if (!have_savings) {
        break;
      }
    }
    for (auto&& pair : prefixes_) {
      prefix_trie->Add(pair.first);
    }
  }

  // Count longest prefixes.
  std::set<std::string> used_prefixes;
  std::vector<std::string> suffix;
  for (const std::string& str : strings) {
    auto pair = prefix_trie->LongestPrefix(str);
    const size_t len = pair.first;
    if (len >= kMinPrefixLen) {
      ++strings_used_prefixed_;
      total_prefix_savings_ += len;
      used_prefixes.insert(str.substr(0, len));
    }
    suffix.push_back(str.substr(len));
    if (suffix.back().size() < kShortLen) {
      ++short_strings_;
    } else {
      ++long_strings_;
    }
  }
  std::sort(suffix.begin(), suffix.end());
  for (const std::string& prefix : used_prefixes) {
    // 4 bytes for an offset, one for length.
    auto pair = prefix_trie->LongestPrefix(prefix);
    CHECK_EQ(pair.first, prefix.length());
    if (pair.second) {
      // Only need to add to dictionary if it's a leaf, otherwise we can reuse string data of the
      // other prefix.
      total_prefix_dict_ += prefix.size();
    }
    total_prefix_table_ += kPrefixConstantCost;
  }
  ProcessStrings(suffix, iterations - 1);
}

void AnalyzeStrings::Dump(std::ostream& os, uint64_t total_size) const {
  os << "Total string data bytes " << Percent(string_data_bytes_, total_size) << "\n";
  os << "UTF-16 string data bytes " << Percent(wide_string_bytes_, total_size) << "\n";
  os << "ASCII string data bytes " << Percent(ascii_string_bytes_, total_size) << "\n";

  // Prefix based strings.
  os << "Total shared prefix bytes " << Percent(total_shared_prefix_bytes_, total_size) << "\n";
  os << "Prefix dictionary cost " << Percent(total_prefix_dict_, total_size) << "\n";
  os << "Prefix table cost " << Percent(total_prefix_table_, total_size) << "\n";
  os << "Prefix index cost " << Percent(total_prefix_index_cost_, total_size) << "\n";
  int64_t net_savings = total_prefix_savings_ + short_strings_;
  net_savings -= total_prefix_dict_;
  net_savings -= total_prefix_table_;
  net_savings -= total_prefix_index_cost_;
  os << "Prefix dictionary elements " << total_num_prefixes_ << "\n";
  os << "Optimization savings " << Percent(optimization_savings_, total_size) << "\n";
  os << "Prefix net savings " << Percent(net_savings, total_size) << "\n";
  os << "Strings using prefix "
     << Percent(strings_used_prefixed_, total_prefix_index_cost_ / kPrefixIndexCost) << "\n";
  os << "Short strings " << Percent(short_strings_, short_strings_ + long_strings_) << "\n";
  if (verbose_level_ >= VerboseLevel::kEverything) {
    std::vector<std::pair<std::string, size_t>> pairs(prefixes_.begin(), prefixes_.end());
    // Sort lexicographically.
    std::sort(pairs.begin(), pairs.end());
    for (const auto& pair : pairs) {
      os << pair.first << " : " << pair.second << "\n";
    }
  }
}

}  // namespace dexanalyze
}  // namespace art
