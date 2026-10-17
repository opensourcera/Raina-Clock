#ifndef TODO_MANAGER_H
#define TODO_MANAGER_H

// Local to-do list: kept in NVS (namespace "todo"), edited on the device (tap to tick) and on the web page /todo.
// No cloud: the list lives on the clock. Up to TODO_MAX_ITEMS items of TODO_MAX_TEXT characters.

#include <Arduino.h>
#include <vector>

#define TODO_MAX_ITEMS 24
#define TODO_MAX_TEXT 64

struct TodoItem {
    String text;
    bool done;
};

class TodoManager {
public:
    void begin();                                   // load the list from NVS
    const std::vector<TodoItem>& items() const { return _items; }
    size_t count() const { return _items.size(); }
    size_t openCount() const;

    bool add(const String& text);                   // trimmed and cut to TODO_MAX_TEXT; false when empty or full
    bool toggle(size_t index);
    bool remove(size_t index);
    bool move(size_t index, int delta);             // -1 up, +1 down
    bool setText(size_t index, const String& text);
    void clearDone();
    void clearAll();

    String toJson() const;                          // {"items":[{"text":"...","done":false}, ...]}
    bool fromJson(const String& json);              // replaces the whole list; false and unchanged on bad JSON
    void save();                                    // write to NVS
    bool consumeChanged();                          // true once after a change, for the UI refresh

private:
    std::vector<TodoItem> _items;
    bool _changed = false;
    static String cleanText(const String& text);
};

extern TodoManager todo;

#endif // TODO_MANAGER_H
