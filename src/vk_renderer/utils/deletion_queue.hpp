#pragma once
#include <deque>

#include <functional>
namespace dt {
class DeletionQueue {
  public:
    std::deque<std::function<void()>> _deletors;
    void push_function(std::function<void()>&& function) { _deletors.push_back(function); }
    void flush() {
        for (auto it = _deletors.rbegin(); it != _deletors.rend(); it++) {
            (*it)();
        }

        _deletors.clear();
    }
};

} // namespace dt
