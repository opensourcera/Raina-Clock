#ifndef TODO_MANAGER_H
#define TODO_MANAGER_H

// Local to-do list, kept like a bullet journal: every task belongs to a day, can be marked urgent, and is ticked off
// when done. Stored in /todo.json on the flash file system (an older list in NVS is taken over once). Edited on the
// device (tap to tick) and on the phone page /todo. No cloud: the list lives on the clock.

#include <Arduino.h>
#include <vector>

#define TODO_MAX_ITEMS 40
#define TODO_MAX_TEXT 64

struct TodoItem {
    String text;
    bool done = false;
    bool urgent = false;
    String day;                 // "2026-10-18"; empty: no day (shown every day until done)
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

    bool hideDone() const { return _hideDone; }
    String toJson() const;                          // {"hide_done":false,"items":[{"text":"...","done":false,"urgent":false,"day":"2026-10-18"}]}
    bool fromJson(const String& json);              // replaces the whole list; false and unchanged on bad JSON
    void save();                                    // write /todo.json
    bool consumeChanged();                          // true once after a change, for the UI refresh

private:
    std::vector<TodoItem> _items;
    bool _changed = false;
    bool _hideDone = false;
    static String cleanText(const String& text);
};

extern TodoManager todo;

#endif // TODO_MANAGER_H
