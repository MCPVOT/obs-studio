/* The hotkey name table's macOS keypad names (obs-rust/obs-studio#870).
 * Included inside obs_hotkeys_set_translations_s() after ADD_TRANSLATION;
 * guarded by its own OS, no include guard. */

#ifdef __APPLE__
#define OBS_HOTKEY_NUMPAD_HOST
numpad_str = t.apple_keypad_num;
ADD_TRANSLATION(OBS_KEY_NUMSLASH, apple_keypad_divide);
ADD_TRANSLATION(OBS_KEY_NUMASTERISK, apple_keypad_multiply);
ADD_TRANSLATION(OBS_KEY_NUMMINUS, apple_keypad_minus);
ADD_TRANSLATION(OBS_KEY_NUMPLUS, apple_keypad_plus);
ADD_TRANSLATION(OBS_KEY_NUMPERIOD, apple_keypad_decimal);
ADD_TRANSLATION(OBS_KEY_NUMEQUAL, apple_keypad_equal);
#endif
