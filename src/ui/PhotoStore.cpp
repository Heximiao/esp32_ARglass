#include "PhotoStore.h"
#include <LittleFS.h>
#include <esp_heap_caps.h>
#include <algorithm>
#include <cstring>

namespace ui {
PhotoStore& photos() { static PhotoStore store; return store; }
void PhotoStore::begin() {
    // Called once at boot. Assets stay alive while LVGL uses their descriptors.
    if (!LittleFS.begin(false)) {
        status_ = "FILESYSTEM OFFLINE";
        Serial.println("LittleFS mount failed; UI will start without photos. Upload filesystem.");
        return;
    }
    photos_.reserve(24);
    File root = LittleFS.open("/");
    size_t totalBytes = 0;
    for (File file = root.openNextFile(); file; file = root.openNextFile()) {
        const String path = file.name();
        if (file.isDirectory() || !path.endsWith(".rgb565") || photos_.size() == 24) continue;
        uint8_t header[8];
        if (file.read(header, 8) != 8 || memcmp(header, "R565", 4) != 0) continue;
        const uint16_t w = header[4] | uint16_t(header[5]) << 8;
        const uint16_t h = header[6] | uint16_t(header[7]) << 8;
        const size_t bytes = size_t(w) * h * 2;
        if (!w || !h || w > 320 || h > 172 || file.size() != bytes + 8 ||
            totalBytes + bytes > 2 * 1024 * 1024) continue;
        auto* data = static_cast<uint8_t*>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (!data) { Serial.println("Photo memory limit reached"); break; }
        if (file.read(data, bytes) != bytes) { heap_caps_free(data); continue; }
        Photo photo;
        photo.name = path.substring(path.lastIndexOf('/') + 1);
        photo.image.header.cf = LV_IMG_CF_TRUE_COLOR;
        photo.image.header.w = w;
        photo.image.header.h = h;
        photo.image.data_size = bytes;
        photo.image.data = data;
        photos_.push_back(photo);
        totalBytes += bytes;
    }
    std::sort(photos_.begin(), photos_.end(), [](const Photo& a, const Photo& b) {
        return a.name.compareTo(b.name) < 0;
    });
    status_ = photos_.empty() ? "NO PHOTOS" : "LOCAL STORAGE";
    Serial.printf("Loaded %u photos (%u bytes)\n", unsigned(count()), unsigned(totalBytes));
}
}
