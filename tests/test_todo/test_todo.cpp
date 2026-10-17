#include <unity.h>
#include "../mocks/mocks.cpp"
#include "../../src/todo_manager.cpp"

void setUp(void) {
    Preferences prefs;
    prefs.begin("todo", false);
    prefs.clear();
    prefs.end();
}

void tearDown(void) {}

void test_todo_starts_empty(void) {
    TodoManager t;
    t.begin();
    TEST_ASSERT_EQUAL(0, t.count());
    TEST_ASSERT_EQUAL(0, t.openCount());
    TEST_ASSERT_TRUE(t.consumeChanged());
    TEST_ASSERT_FALSE(t.consumeChanged());
}

void test_todo_add_toggle_remove(void) {
    TodoManager t;
    t.begin();
    TEST_ASSERT_TRUE(t.add("  Buy milk  "));
    TEST_ASSERT_TRUE(t.add("Call mom"));
    TEST_ASSERT_FALSE(t.add("   "));
    TEST_ASSERT_EQUAL(2, t.count());
    TEST_ASSERT_EQUAL_STRING("Buy milk", t.items()[0].text.c_str());
    TEST_ASSERT_TRUE(t.toggle(0));
    TEST_ASSERT_TRUE(t.items()[0].done);
    TEST_ASSERT_EQUAL(1, t.openCount());
    TEST_ASSERT_FALSE(t.toggle(5));
    TEST_ASSERT_TRUE(t.remove(0));
    TEST_ASSERT_EQUAL(1, t.count());
    TEST_ASSERT_EQUAL_STRING("Call mom", t.items()[0].text.c_str());
}

void test_todo_move_and_clear_done(void) {
    TodoManager t;
    t.begin();
    t.add("a"); t.add("b"); t.add("c");
    TEST_ASSERT_TRUE(t.move(2, -1));
    TEST_ASSERT_EQUAL_STRING("c", t.items()[1].text.c_str());
    TEST_ASSERT_FALSE(t.move(0, -1));
    t.toggle(0); t.toggle(2);
    t.clearDone();
    TEST_ASSERT_EQUAL(1, t.count());
    TEST_ASSERT_EQUAL_STRING("c", t.items()[0].text.c_str());
}

void test_todo_limits(void) {
    TodoManager t;
    t.begin();
    for (int i = 0; i < TODO_MAX_ITEMS; i++) { char b[16]; snprintf(b, sizeof(b), "item %d", i); TEST_ASSERT_TRUE(t.add(String(b))); }
    TEST_ASSERT_FALSE(t.add("one too many"));
    TEST_ASSERT_EQUAL(TODO_MAX_ITEMS, t.count());
    t.clearAll();
    String longText;
    for (int i = 0; i < 100; i++) longText += "x";
    TEST_ASSERT_TRUE(t.add(longText));
    TEST_ASSERT_EQUAL(TODO_MAX_TEXT, t.items()[0].text.length());
}

void test_todo_json_roundtrip(void) {
    TodoManager t;
    t.begin();
    t.add("Water the plants"); t.add("Fix \"the\" sink"); t.toggle(1);
    String json = t.toJson();
    TodoManager u;
    u.begin();
    TEST_ASSERT_TRUE(u.fromJson(json));
    TEST_ASSERT_EQUAL(2, u.count());
    TEST_ASSERT_EQUAL_STRING("Fix \"the\" sink", u.items()[1].text.c_str());
    TEST_ASSERT_TRUE(u.items()[1].done);
    TEST_ASSERT_FALSE(u.items()[0].done);
    TEST_ASSERT_FALSE(u.fromJson("{not json"));
    TEST_ASSERT_EQUAL(2, u.count());
    TEST_ASSERT_FALSE(u.fromJson("{\"other\":1}"));
}

void test_todo_persists(void) {
    TodoManager t;
    t.begin();
    t.add("Persist me"); t.toggle(0);
    t.save();
    TodoManager u;
    u.begin();
    TEST_ASSERT_EQUAL(1, u.count());
    TEST_ASSERT_EQUAL_STRING("Persist me", u.items()[0].text.c_str());
    TEST_ASSERT_TRUE(u.items()[0].done);
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_todo_starts_empty);
    RUN_TEST(test_todo_add_toggle_remove);
    RUN_TEST(test_todo_move_and_clear_done);
    RUN_TEST(test_todo_limits);
    RUN_TEST(test_todo_json_roundtrip);
    RUN_TEST(test_todo_persists);
    return UNITY_END();
}
