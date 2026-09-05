#pragma once
#include <stdint.h>

class QtKeyMapper {
public:
    QtKeyMapper();

    // Translate a Godot Key enum value to a Qt::Key value.
    // Returns 0 if no mapping exists.
    int godot_to_qt(int godot_key) const;

    // Runtime extension: add/remove/reset custom mappings.
    void add_mapping(int godot_key, int qt_key);
    void remove_mapping(int godot_key);
    void reset_to_defaults();

private:
    struct Entry { int godot; int qt; };
    static const Entry BASE_TABLE[];
    static const int   BASE_TABLE_SIZE;

    // Runtime extension: parallel arrays (small, linear scan OK)
    int _ext_godot[64];
    int _ext_qt[64];
    int _ext_count;
};
