#include "todo_manager.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <string>
#include "event_log.h"

TodoManager todo;

static const char* TODO_NS = "todo";
static const char* TODO_KEY = "json";

String TodoManager::cleanText(const String& text) {   // trim by hand: the host test mock of String has no trim()
    const char* s = text.c_str();
    size_t a = 0, b = strlen(s);
    while (a < b && (unsigned char)s[a] <= 32) a++;
    while (b > a && (unsigned char)s[b - 1] <= 32) b--;
    if (b - a > TODO_MAX_TEXT) b = a + TODO_MAX_TEXT;
    char buf[TODO_MAX_TEXT + 1];
    memcpy(buf, s + a, b - a); buf[b - a] = 0;
    return String(buf);
}

void TodoManager::begin() {
    String json = fsReadFile("/todo.json");
    if (json.length() == 0) {                       // the first start with this version: take over the old list
        Preferences prefs;
        prefs.begin(TODO_NS, true);
        json = prefs.getString(TODO_KEY, "");
        prefs.end();
    }
    _items.clear();
    if (json.length() > 0) fromJson(json);
    _changed = true;
}

size_t TodoManager::openCount() const {
    size_t n = 0;
    for (const auto& it : _items) if (!it.done) n++;
    return n;
}

bool TodoManager::add(const String& text) {
    String t = cleanText(text);
    if (t.length() == 0 || _items.size() >= TODO_MAX_ITEMS) return false;
    TodoItem it; it.text = t;
    _items.push_back(it);
    _changed = true;
    return true;
}

bool TodoManager::toggle(size_t index) {
    if (index >= _items.size()) return false;
    _items[index].done = !_items[index].done;
    _changed = true;
    return true;
}

bool TodoManager::remove(size_t index) {
    if (index >= _items.size()) return false;
    _items.erase(_items.begin() + index);
    _changed = true;
    return true;
}

bool TodoManager::move(size_t index, int delta) {
    if (index >= _items.size()) return false;
    int target = (int)index + delta;
    if (target < 0 || target >= (int)_items.size()) return false;
    TodoItem tmp = _items[index];
    _items[index] = _items[target];
    _items[target] = tmp;
    _changed = true;
    return true;
}

bool TodoManager::setText(size_t index, const String& text) {
    String t = cleanText(text);
    if (index >= _items.size() || t.length() == 0) return false;
    _items[index].text = t;
    _changed = true;
    return true;
}

void TodoManager::clearDone() {
    for (size_t i = _items.size(); i-- > 0;) if (_items[i].done) _items.erase(_items.begin() + i);
    _changed = true;
}

void TodoManager::clearAll() {
    _items.clear();
    _changed = true;
}

String TodoManager::toJson() const {
    DynamicJsonDocument doc(6144);
    doc["hide_done"] = _hideDone;
    JsonArray arr = doc.createNestedArray("items");
    for (const auto& it : _items) {
        JsonObject o = arr.createNestedObject();
        o["text"] = it.text.c_str();
        o["done"] = it.done;
        if (it.urgent) o["urgent"] = true;
        if (it.day.length()) o["day"] = it.day.c_str();
    }
    std::string out;
    serializeJson(doc, out);
    return String(out.c_str());
}

bool TodoManager::fromJson(const String& json) {
    DynamicJsonDocument doc(6144);
    if (deserializeJson(doc, json.c_str())) return false;
    JsonArray arr = doc["items"].as<JsonArray>();
    if (arr.isNull()) return false;
    std::vector<TodoItem> fresh;
    for (JsonObject o : arr) {
        if (fresh.size() >= TODO_MAX_ITEMS) break;
        String t = cleanText(String(o["text"] | ""));
        if (t.length() == 0) continue;
        TodoItem it;
        it.text = t;
        it.done = o["done"] | false;
        it.urgent = o["urgent"] | false;
        const char* d = o["day"] | "";
        if (strlen(d) == 10) it.day = d;
        fresh.push_back(it);
    }
    _items = fresh;
    _hideDone = doc["hide_done"] | false;
    _changed = true;
    return true;
}

void TodoManager::save() {
    if (!fsWriteFile("/todo.json", toJson())) eventLog("todo: could not write /todo.json");
}

bool TodoManager::consumeChanged() {
    bool c = _changed;
    _changed = false;
    return c;
}
