#include <common/input/input_mapping.h>

char const* toString(KeyCode keycode) noexcept {
    switch (keycode) {
    case KeyCode::Unknown:
        return "Unknown";
    case KeyCode::Space:
        return "Space";
    case KeyCode::Apostrophe:
        return "Apostrophe";
    case KeyCode::Comma:
        return "Comma";
    case KeyCode::Minus:
        return "Minus";
    case KeyCode::Period:
        return "Period";
    case KeyCode::Slash:
        return "Slash";
    case KeyCode::Alpha0:
        return "Alpha0";
    case KeyCode::Alpha1:
        return "Alpha1";
    case KeyCode::Alpha2:
        return "Alpha2";
    case KeyCode::Alpha3:
        return "Alpha3";
    case KeyCode::Alpha4:
        return "Alpha4";
    case KeyCode::Alpha5:
        return "Alpha5";
    case KeyCode::Alpha6:
        return "Alpha6";
    case KeyCode::Alpha7:
        return "Alpha7";
    case KeyCode::Alpha8:
        return "Alpha8";
    case KeyCode::Alpha9:
        return "Alpha9";
    case KeyCode::Semicolon:
        return "Semicolon";
    case KeyCode::Equal:
        return "Equal";
    case KeyCode::A:
        return "A";
    case KeyCode::B:
        return "B";
    case KeyCode::C:
        return "C";
    case KeyCode::D:
        return "D";
    case KeyCode::E:
        return "E";
    case KeyCode::F:
        return "F";
    case KeyCode::G:
        return "G";
    case KeyCode::H:
        return "H";
    case KeyCode::I:
        return "I";
    case KeyCode::J:
        return "J";
    case KeyCode::K:
        return "K";
    case KeyCode::L:
        return "L";
    case KeyCode::M:
        return "M";
    case KeyCode::N:
        return "N";
    case KeyCode::O:
        return "O";
    case KeyCode::P:
        return "P";
    case KeyCode::Q:
        return "Q";
    case KeyCode::R:
        return "R";
    case KeyCode::S:
        return "S";
    case KeyCode::T:
        return "T";
    case KeyCode::U:
        return "U";
    case KeyCode::V:
        return "V";
    case KeyCode::W:
        return "W";
    case KeyCode::X:
        return "X";
    case KeyCode::Y:
        return "Y";
    case KeyCode::Z:
        return "Z";
    case KeyCode::LeftBracket:
        return "LeftBracket";
    case KeyCode::Backslash:
        return "Backslash";
    case KeyCode::RightBracket:
        return "RightBracket";
    case KeyCode::Backquote:
        return "Backquote";
    case KeyCode::Escape:
        return "Escape";
    case KeyCode::Enter:
        return "Enter";
    case KeyCode::Tab:
        return "Tab";
    case KeyCode::Backspace:
        return "Backspace";
    case KeyCode::Insert:
        return "Insert";
    case KeyCode::Delete:
        return "Delete";
    case KeyCode::Right:
        return "Right";
    case KeyCode::Left:
        return "Left";
    case KeyCode::Down:
        return "Down";
    case KeyCode::Up:
        return "Up";
    case KeyCode::PageUp:
        return "PageUp";
    case KeyCode::PageDown:
        return "PageDown";
    case KeyCode::Home:
        return "Home";
    case KeyCode::End:
        return "End";
    case KeyCode::CapsLock:
        return "CapsLock";
    case KeyCode::ScrollLock:
        return "ScrollLock";
    case KeyCode::NumLock:
        return "NumLock";
    case KeyCode::PrintScreen:
        return "PrintScreen";
    case KeyCode::Pause:
        return "Pause";
    case KeyCode::F1:
        return "F1";
    case KeyCode::F2:
        return "F2";
    case KeyCode::F3:
        return "F3";
    case KeyCode::F4:
        return "F4";
    case KeyCode::F5:
        return "F5";
    case KeyCode::F6:
        return "F6";
    case KeyCode::F7:
        return "F7";
    case KeyCode::F8:
        return "F8";
    case KeyCode::F9:
        return "F9";
    case KeyCode::F10:
        return "F10";
    case KeyCode::F11:
        return "F11";
    case KeyCode::F12:
        return "F12";
    case KeyCode::F13:
        return "F13";
    case KeyCode::F14:
        return "F14";
    case KeyCode::F15:
        return "F15";
    case KeyCode::F16:
        return "F16";
    case KeyCode::F17:
        return "F17";
    case KeyCode::F18:
        return "F18";
    case KeyCode::F19:
        return "F19";
    case KeyCode::F20:
        return "F20";
    case KeyCode::F21:
        return "F21";
    case KeyCode::F22:
        return "F22";
    case KeyCode::F23:
        return "F23";
    case KeyCode::F24:
        return "F24";
    case KeyCode::F25:
        return "F25";
    case KeyCode::Keypad0:
        return "Keypad0";
    case KeyCode::Keypad1:
        return "Keypad1";
    case KeyCode::Keypad2:
        return "Keypad2";
    case KeyCode::Keypad3:
        return "Keypad3";
    case KeyCode::Keypad4:
        return "Keypad4";
    case KeyCode::Keypad5:
        return "Keypad5";
    case KeyCode::Keypad6:
        return "Keypad6";
    case KeyCode::Keypad7:
        return "Keypad7";
    case KeyCode::Keypad8:
        return "Keypad8";
    case KeyCode::Keypad9:
        return "Keypad9";
    case KeyCode::KeypadDecimal:
        return "KeypadDecimal";
    case KeyCode::KeypadDivide:
        return "KeypadDivide";
    case KeyCode::KeypadMultiply:
        return "KeypadMultiply";
    case KeyCode::KeypadSubtract:
        return "KeypadSubtract";
    case KeyCode::KeypadAdd:
        return "KeypadAdd";
    case KeyCode::KeypadEnter:
        return "KeypadEnter";
    case KeyCode::KeypadEqual:
        return "KeypadEqual";
    case KeyCode::LeftShift:
        return "LeftShift";
    case KeyCode::LeftControl:
        return "LeftControl";
    case KeyCode::LeftAlt:
        return "LeftAlt";
    case KeyCode::LeftSuper:
        return "LeftSuper";
    case KeyCode::RightShift:
        return "RightShift";
    case KeyCode::RightControl:
        return "RightControl";
    case KeyCode::RightAlt:
        return "RightAlt";
    case KeyCode::RightSuper:
        return "RightSuper";
    case KeyCode::Menu:
        return "Menu";
    }

    return "Unknown";
}

char const* toString(KeyState state) noexcept {
    switch (state) {
    case KeyState::None:
        return "None";
    case KeyState::Pressed:
        return "Pressed";
    case KeyState::Held:
        return "Held";
    case KeyState::Released:
        return "Released";
    }

    return "Unknown";
}

char const* toString(MouseButton button) noexcept {
    switch (button) {
    case MouseButton::Unknown:
        return "Unknown";
    case MouseButton::Left:
        return "Left";
    case MouseButton::Right:
        return "Right";
    case MouseButton::Middle:
        return "Middle";
    }

    return "Unknown";
}

char const* toString(MouseButtonState state) noexcept {
    switch (state) {
    case MouseButtonState::None:
        return "None";
    case MouseButtonState::Pressed:
        return "Pressed";
    case MouseButtonState::Held:
        return "Held";
    case MouseButtonState::Released:
        return "Released";
    }

    return "Unknown";
}

char const* toString(CursorMode mode) noexcept {
    switch (mode) {
    case CursorMode::Normal:
        return "Normal";
    case CursorMode::Hidden:
        return "Hidden";
    case CursorMode::Disabled:
        return "Disabled";
    }

    return "Unknown";
}
