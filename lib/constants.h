#ifndef CONSTANTS_H_
#define CONSTANTS_H_

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT {NULL, 0}
#define TAB_STOP 8

#define HL_NUMBERS (1 << 0)
#define HL_STRINGS (1 << 1)

// map for special keys
enum specialKeys {
BACKSPACE = 127,
ARROW_LEFT = 1000,
ARROW_RIGHT,
ARROW_UP,
ARROW_DOWN,
HOME_KEY,
END_KEY,
DEL_KEY,
PAGE_UP,
PAGE_DOWN
};

enum highlightVals {
HL_NORMAL = 0,
HL_STRING,
HL_COMMENT,
HL_MCOMMENT,
HL_KEYWORD1,
HL_KEYWORD2,
HL_NUMBER,
HL_SEARCH
};


#endif // CONSTANTS_H_
