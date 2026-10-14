#pragma once
#include <lvgl.h>
#include <Arduino.h>
#include <vector>

namespace ui {
struct Photo {
    String name;
    lv_img_dsc_t image{};
};
class PhotoStore {
public:
    void begin();
    size_t count() const { return photos_.size(); }
    const Photo* at(size_t index) const { return index < count() ? &photos_[index] : nullptr; }
    const char* status() const { return status_; }
private:
    std::vector<Photo> photos_;
    const char* status_ = "NO PHOTOS";
};
PhotoStore& photos();
}
