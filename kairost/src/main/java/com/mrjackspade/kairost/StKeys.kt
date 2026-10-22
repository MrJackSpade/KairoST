package com.mrjackspade.kairost

import com.mrjackspade.kairo.frontend.ControllerBinding
import com.mrjackspade.kairo.frontend.ControllerBindingsCodec
import com.mrjackspade.kairo.frontend.ControllerLayout
import com.mrjackspade.kairo.frontend.GuestKeyboardLayout
import com.mrjackspade.kairo.frontend.GuestKeyboardPage
import com.mrjackspade.kairo.frontend.JoystickInputRouter
import com.mrjackspade.kairo.frontend.KeyboardKey
import android.view.KeyEvent

/**
 * The Atari ST keyboard by its IKBD scan codes (the core's keys, 1 to 0x72), with the US
 * legends, and the joystick in port 1.
 */
object StKeys {
    const val ESC = 0x01
    /** Digits 1 to 9, then 0: 0x02 to 0x0B. */
    private fun digit(char: Char) = if (char == '0') 0x0B else 0x02 + (char - '1')
    const val MINUS = 0x0C
    const val EQUALS = 0x0D
    const val BACKSPACE = 0x0E
    const val TAB = 0x0F
    const val LEFT_BRACKET = 0x1A
    const val RIGHT_BRACKET = 0x1B
    const val RETURN = 0x1C
    const val CONTROL = 0x1D
    const val SEMICOLON = 0x27
    const val QUOTE = 0x28
    const val BACKQUOTE = 0x29
    const val LEFT_SHIFT = 0x2A
    const val BACKSLASH = 0x2B
    const val COMMA = 0x33
    const val DOT = 0x34
    const val SLASH = 0x35
    const val RIGHT_SHIFT = 0x36
    const val ALTERNATE = 0x38
    const val SPACE = 0x39
    const val CAPS = 0x3A
    /** F1 to F10: 0x3B to 0x44. */
    private fun function(number: Int) = 0x3B + number - 1
    const val CLR_HOME = 0x47
    const val UP = 0x48
    const val NUM_MINUS = 0x4A
    const val LEFT = 0x4B
    const val RIGHT = 0x4D
    const val NUM_PLUS = 0x4E
    const val DOWN = 0x50
    const val INSERT = 0x52
    const val DELETE = 0x53
    /** The ISO key left of Z on European keyboards. */
    const val ISO = 0x60
    const val UNDO = 0x61
    const val HELP = 0x62
    const val NUM_OPEN = 0x63
    const val NUM_CLOSE = 0x64
    const val NUM_SLASH = 0x65
    const val NUM_STAR = 0x66
    /** Keypad 7 8 9 4 5 6 1 2 3 0 from 0x67. */
    private fun numpad(digit: Int) = if (digit == 0) 0x70 else 0x67 + listOf(7, 8, 9, 4, 5, 6, 1, 2, 3).indexOf(digit)
    const val NUM_DOT = 0x71
    const val NUM_ENTER = 0x72
    const val MAX_CODE = 0x72

    private const val DIGITS = "1234567890"
    /** Letters by row, from Q (0x10), A (0x1E) and Z (0x2C). */
    private val letterCodes: Map<Char, Int> = "qwertyuiop".mapIndexed { i, c -> c to 0x10 + i }.toMap() +
        "asdfghjkl".mapIndexed { i, c -> c to 0x1E + i } + "zxcvbnm".mapIndexed { i, c -> c to 0x2C + i }
    private fun letter(char: Char) = letterCodes.getValue(char.lowercaseChar())

    val layout = GuestKeyboardLayout(
        brand = "KAIROST",
        pages = listOf(
            GuestKeyboardPage("ABC", alphabetRows()),
            GuestKeyboardPage("?123", symbolRows(), pickerLabel = "Symbols"),
            GuestKeyboardPage("ST", stRows())
        ),
        modifiers = setOf(LEFT_SHIFT, RIGHT_SHIFT, CONTROL, ALTERNATE, CAPS),
        shiftCodes = setOf(LEFT_SHIFT, RIGHT_SHIFT),
        capsCode = CAPS,
        chordShiftCode = LEFT_SHIFT
    )

    private fun alphabetRows(): List<List<KeyboardKey>> = listOf(
        listOf(KeyboardKey("Esc", ESC, 1.3f)) +
            DIGITS.zip("!@#$%^&*()").map { (char, shifted) -> KeyboardKey("$char", digit(char), shifted = "$shifted") } +
            listOf(KeyboardKey("-", MINUS, shifted = "_"), KeyboardKey("=", EQUALS, shifted = "+"),
                KeyboardKey("`", BACKQUOTE, shifted = "~"), KeyboardKey("BS", BACKSPACE, 1.5f)),
        listOf(KeyboardKey("Tab", TAB, 1.3f)) + "qwertyuiop".map { KeyboardKey("${it.uppercaseChar()}", letter(it)) } +
            listOf(KeyboardKey("[", LEFT_BRACKET, shifted = "{"), KeyboardKey("]", RIGHT_BRACKET, shifted = "}"),
                KeyboardKey("Return", RETURN, 1.5f)),
        listOf(KeyboardKey("Ctrl", CONTROL, 1.4f)) + "asdfghjkl".map { KeyboardKey("${it.uppercaseChar()}", letter(it)) } +
            listOf(KeyboardKey(";", SEMICOLON, shifted = ":"), KeyboardKey("'", QUOTE, shifted = "\""),
                KeyboardKey("\\", BACKSLASH, shifted = "|")),
        listOf(KeyboardKey("Shift", LEFT_SHIFT, 1.5f)) + "zxcvbnm".map { KeyboardKey("${it.uppercaseChar()}", letter(it)) } +
            listOf(KeyboardKey(",", COMMA, shifted = "<"), KeyboardKey(".", DOT, shifted = ">"),
                KeyboardKey("/", SLASH, shifted = "?"), KeyboardKey("Shift", RIGHT_SHIFT, 1.5f)),
        listOf(KeyboardKey("Caps", CAPS), KeyboardKey("Alt", ALTERNATE), KeyboardKey("Space", SPACE, 5f),
            KeyboardKey("Help", HELP), KeyboardKey("←", LEFT), KeyboardKey("↑", UP), KeyboardKey("↓", DOWN),
            KeyboardKey("→", RIGHT))
    )

    /** Each symbol on its own key, Shift chorded where the symbol needs it. */
    private fun symbolRows(): List<List<KeyboardKey>> = listOf(
        listOf(KeyboardKey("Esc", ESC, 1.3f)) + DIGITS.map { KeyboardKey("$it", digit(it)) } + KeyboardKey("BS", BACKSPACE, 1.5f),
        DIGITS.zip("!@#$%^&*()").map { (char, symbol) -> KeyboardKey("$symbol", digit(char), chordShift = true) } +
            KeyboardKey("Return", RETURN, 1.5f),
        listOf("-" to MINUS, "=" to EQUALS, "\\" to BACKSLASH, "[" to LEFT_BRACKET, "]" to RIGHT_BRACKET,
            ";" to SEMICOLON, "'" to QUOTE, "`" to BACKQUOTE, "," to COMMA, "." to DOT, "/" to SLASH)
            .map { (label, code) -> KeyboardKey(label, code) },
        listOf("_" to MINUS, "+" to EQUALS, "|" to BACKSLASH, "{" to LEFT_BRACKET, "}" to RIGHT_BRACKET,
            ":" to SEMICOLON, "\"" to QUOTE, "~" to BACKQUOTE, "<" to COMMA, ">" to DOT, "?" to SLASH)
            .map { (label, code) -> KeyboardKey(label, code, chordShift = true) },
        listOf(KeyboardKey("Tab", TAB), KeyboardKey("Space", SPACE, 5f), KeyboardKey("Return", RETURN, 1.5f))
    )

    /** The function keys, the editing block and the keypad. */
    private fun stRows(): List<List<KeyboardKey>> = listOf(
        (1..10).map { KeyboardKey("F$it", function(it)) },
        listOf(KeyboardKey("Help", HELP), KeyboardKey("Undo", UNDO), KeyboardKey("Ins", INSERT),
            KeyboardKey("Clr", CLR_HOME), KeyboardKey("Del", DELETE), KeyboardKey("↑", UP),
            KeyboardKey("(", NUM_OPEN), KeyboardKey(")", NUM_CLOSE), KeyboardKey("/", NUM_SLASH), KeyboardKey("*", NUM_STAR)),
        listOf(KeyboardKey("Ctrl", CONTROL), KeyboardKey("Alt", ALTERNATE), KeyboardKey("←", LEFT),
            KeyboardKey("↓", DOWN), KeyboardKey("→", RIGHT), KeyboardKey("7", numpad(7)), KeyboardKey("8", numpad(8)),
            KeyboardKey("9", numpad(9)), KeyboardKey("-", NUM_MINUS)),
        listOf(KeyboardKey("<>", ISO), KeyboardKey("Shift", LEFT_SHIFT), KeyboardKey("4", numpad(4)),
            KeyboardKey("5", numpad(5)), KeyboardKey("6", numpad(6)), KeyboardKey("+", NUM_PLUS)),
        listOf(KeyboardKey("1", numpad(1)), KeyboardKey("2", numpad(2)), KeyboardKey("3", numpad(3)),
            KeyboardKey("0", numpad(0)), KeyboardKey(".", NUM_DOT), KeyboardKey("Enter", NUM_ENTER))
    )

    /** Every key a controller binding can send. */
    val bindableCodes: List<Int> = (1..MAX_CODE).filter { code -> label(code) != null }

    fun label(code: Int): String? = when (code) {
        ESC -> "Esc"
        in DIGITS.map(::digit) -> "${DIGITS[DIGITS.map(::digit).indexOf(code)]}"
        MINUS -> "-"
        EQUALS -> "="
        BACKSPACE -> "Backspace"
        TAB -> "Tab"
        in letterCodes.values -> "${letterCodes.entries.first { it.value == code }.key.uppercaseChar()}"
        LEFT_BRACKET -> "["
        RIGHT_BRACKET -> "]"
        RETURN -> "Return"
        CONTROL -> "Control"
        SEMICOLON -> ";"
        QUOTE -> "'"
        BACKQUOTE -> "`"
        LEFT_SHIFT -> "Left Shift"
        BACKSLASH -> "\\"
        COMMA -> ","
        DOT -> "."
        SLASH -> "/"
        RIGHT_SHIFT -> "Right Shift"
        ALTERNATE -> "Alternate"
        SPACE -> "Space"
        CAPS -> "Caps Lock"
        in (1..10).map(::function) -> "F${code - function(1) + 1}"
        CLR_HOME -> "Clr Home"
        UP -> "Up"
        NUM_MINUS -> "Keypad -"
        LEFT -> "Left"
        RIGHT -> "Right"
        NUM_PLUS -> "Keypad +"
        DOWN -> "Down"
        INSERT -> "Insert"
        DELETE -> "Delete"
        ISO -> "< >"
        UNDO -> "Undo"
        HELP -> "Help"
        NUM_OPEN -> "Keypad ("
        NUM_CLOSE -> "Keypad )"
        NUM_SLASH -> "Keypad /"
        NUM_STAR -> "Keypad *"
        in (0..9).map(::numpad) -> "Keypad ${(0..9).first { numpad(it) == code }}"
        NUM_DOT -> "Keypad ."
        NUM_ENTER -> "Keypad Enter"
        else -> null
    }

    /** The ST key an Android key types, by its US legend; Page Up and Page Down are Help and Undo. */
    fun fromAndroid(key: Int): Int? {
        if (key in KeyEvent.KEYCODE_A..KeyEvent.KEYCODE_Z) return letter('a' + (key - KeyEvent.KEYCODE_A))
        if (key in KeyEvent.KEYCODE_0..KeyEvent.KEYCODE_9) return digit('0' + (key - KeyEvent.KEYCODE_0))
        if (key in KeyEvent.KEYCODE_F1..KeyEvent.KEYCODE_F10) return function(key - KeyEvent.KEYCODE_F1 + 1)
        if (key in KeyEvent.KEYCODE_NUMPAD_0..KeyEvent.KEYCODE_NUMPAD_9) return numpad(key - KeyEvent.KEYCODE_NUMPAD_0)
        return when (key) {
            KeyEvent.KEYCODE_ESCAPE -> ESC
            KeyEvent.KEYCODE_MINUS -> MINUS
            KeyEvent.KEYCODE_EQUALS -> EQUALS
            KeyEvent.KEYCODE_DEL -> BACKSPACE
            KeyEvent.KEYCODE_TAB -> TAB
            KeyEvent.KEYCODE_LEFT_BRACKET -> LEFT_BRACKET
            KeyEvent.KEYCODE_RIGHT_BRACKET -> RIGHT_BRACKET
            KeyEvent.KEYCODE_ENTER -> RETURN
            KeyEvent.KEYCODE_CTRL_LEFT, KeyEvent.KEYCODE_CTRL_RIGHT -> CONTROL
            KeyEvent.KEYCODE_SEMICOLON -> SEMICOLON
            KeyEvent.KEYCODE_APOSTROPHE -> QUOTE
            KeyEvent.KEYCODE_GRAVE -> BACKQUOTE
            KeyEvent.KEYCODE_SHIFT_LEFT -> LEFT_SHIFT
            KeyEvent.KEYCODE_BACKSLASH -> BACKSLASH
            KeyEvent.KEYCODE_COMMA -> COMMA
            KeyEvent.KEYCODE_PERIOD -> DOT
            KeyEvent.KEYCODE_SLASH -> SLASH
            KeyEvent.KEYCODE_SHIFT_RIGHT -> RIGHT_SHIFT
            KeyEvent.KEYCODE_ALT_LEFT, KeyEvent.KEYCODE_ALT_RIGHT -> ALTERNATE
            KeyEvent.KEYCODE_SPACE -> SPACE
            KeyEvent.KEYCODE_CAPS_LOCK -> CAPS
            KeyEvent.KEYCODE_MOVE_HOME -> CLR_HOME
            KeyEvent.KEYCODE_DPAD_UP -> UP
            KeyEvent.KEYCODE_NUMPAD_SUBTRACT -> NUM_MINUS
            KeyEvent.KEYCODE_DPAD_LEFT -> LEFT
            KeyEvent.KEYCODE_DPAD_RIGHT -> RIGHT
            KeyEvent.KEYCODE_NUMPAD_ADD -> NUM_PLUS
            KeyEvent.KEYCODE_DPAD_DOWN -> DOWN
            KeyEvent.KEYCODE_INSERT -> INSERT
            KeyEvent.KEYCODE_FORWARD_DEL -> DELETE
            KeyEvent.KEYCODE_PAGE_DOWN -> UNDO
            KeyEvent.KEYCODE_PAGE_UP -> HELP
            KeyEvent.KEYCODE_NUMPAD_LEFT_PAREN -> NUM_OPEN
            KeyEvent.KEYCODE_NUMPAD_RIGHT_PAREN -> NUM_CLOSE
            KeyEvent.KEYCODE_NUMPAD_DIVIDE -> NUM_SLASH
            KeyEvent.KEYCODE_NUMPAD_MULTIPLY -> NUM_STAR
            KeyEvent.KEYCODE_NUMPAD_DOT -> NUM_DOT
            KeyEvent.KEYCODE_NUMPAD_ENTER -> NUM_ENTER
            else -> null
        }
    }

    /**
     * The joystick's controls in the router's order. The core's joystick (st_machine.h) has the
     * first five; the ST joystick has one fire button, so "button2" reaches nothing.
     */
    val joystick = JoystickInputRouter.DEFAULT_CONTROLS
    val joystickLabels = listOf("Up", "Down", "Left", "Right", "Fire", "Fire 2 (unused)")

    val bindings = ControllerBindingsCodec(::defaults, { it in bindableCodes }, joystick,
        setOf("menu", "pause", "restart", "exit", "fastForward"))

    fun defaults(layout: ControllerLayout): List<ControllerBinding> =
        if (layout == ControllerLayout.WITH_STICKS) defaults()
        else defaults().filterNot { it.input.startsWith("virtual:ls") || it.input.startsWith("virtual:rs") }

    /**
     * ST games read the joystick in port 1, the mouse in port 0, or the keyboard: the D-pad and A
     * are the joystick, the left stick moves the mouse with L1 and R1 its buttons, B, X and Y are
     * Space, Return and Esc, Start is F1 (many games' start or pause), Select is Help, and the
     * right stick is the cursor keys.
     */
    fun defaults() = listOf(
        ControllerBinding("virtual:a", joystick = "button1"),
        ControllerBinding("virtual:b", listOf(SPACE)),
        ControllerBinding("virtual:x", listOf(RETURN)),
        ControllerBinding("virtual:y", listOf(ESC)),
        ControllerBinding("virtual:l1", mouse = "leftButton"),
        ControllerBinding("virtual:r1", mouse = "rightButton"),
        ControllerBinding("virtual:start", listOf(function(1))),
        ControllerBinding("virtual:select", listOf(HELP)),
        ControllerBinding("virtual:menu", action = "menu"),
        ControllerBinding("virtual:up", joystick = "up"),
        ControllerBinding("virtual:down", joystick = "down"),
        ControllerBinding("virtual:left", joystick = "left"),
        ControllerBinding("virtual:right", joystick = "right"),
        ControllerBinding("virtual:lsup", mouse = "moveUp"),
        ControllerBinding("virtual:lsdown", mouse = "moveDown"),
        ControllerBinding("virtual:lsleft", mouse = "moveLeft"),
        ControllerBinding("virtual:lsright", mouse = "moveRight"),
        ControllerBinding("virtual:rsup", listOf(UP)),
        ControllerBinding("virtual:rsdown", listOf(DOWN)),
        ControllerBinding("virtual:rsleft", listOf(LEFT)),
        ControllerBinding("virtual:rsright", listOf(RIGHT))
    )
}
