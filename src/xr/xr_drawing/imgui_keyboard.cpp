/*
  This file is part of KDGpu.

  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "imgui_keyboard.h"

#include <cctype>
#include <cstring>

ImGuiKeyboard::ImGuiKeyboard()
    : m_shiftMode(ShiftMode::Off)
    , m_modifiers(KDGui::KeyboardModifier::Mod_NoModifiers)
{
}

char ImGuiKeyboard::getShiftedChar(char c)
{
    switch (c) {
    case '1':
        return '!';
    case '2':
        return '@';
    case '3':
        return '#';
    case '4':
        return '$';
    case '5':
        return '%';
    case '6':
        return '^';
    case '7':
        return '&';
    case '8':
        return '*';
    case '9':
        return '(';
    case '0':
        return ')';
    case '-':
        return '_';
    case '=':
        return '+';
    case '[':
        return '{';
    case ']':
        return '}';
    case '\\':
        return '|';
    case ';':
        return ':';
    case '\'':
        return '"';
    case ',':
        return '<';
    case '.':
        return '>';
    case '/':
        return '?';
    default:
        return std::toupper(c);
    }
}

void ImGuiKeyboard::renderKey(const char *label, char outputChar, float keyWidth, float keyHeight, std::string &inputText)
{
    if (ImGui::Button(label, ImVec2(keyWidth, keyHeight))) {
        inputText += outputChar;
        m_pressedKey = static_cast<KDGui::Key>(outputChar);
        if (m_shiftMode == ShiftMode::Shift)
            m_shiftMode = ShiftMode::Off;
    }
    ImGui::SameLine();
}

void ImGuiKeyboard::renderKeyboard(std::string &inputText)
{
    const char *row1 = "1234567890-=";
    const char *row2 = "qwertyuiop[]";
    const char *row3 = "asdfghjkl;'";
    const char *row4 = "zxcvbnm,.";

    auto isShift = m_shiftMode != ShiftMode::Off;
    m_pressedKey = std::nullopt;

    // Calculate key size
    auto keyWidth = ImGui::GetFrameHeight() * 1.5f; // Normalized width
    auto keyHeight = ImGui::GetFrameHeight(); // Normalized height
    auto rowIndent = ImGui::GetFrameHeight() * 0.75f;
    auto keySpacing = ImGui::GetStyle().ItemSpacing.x;

    // Number Row
    for (const char *p = row1; *p != '\0'; ++p) {
        char displayChar = (isShift) ? getShiftedChar(*p) : *p;
        renderKey(std::string(1, displayChar).c_str(), displayChar, keyWidth, keyHeight, inputText);
    }

    // Second Row
    ImGui::NewLine();
    ImGui::Indent(rowIndent);
    for (const char *p = row2; *p != '\0'; ++p) {
        char displayChar = (isShift) ? getShiftedChar(*p) : *p;
        renderKey(std::string(1, displayChar).c_str(), displayChar, keyWidth, keyHeight, inputText);
    }

    // Third Row
    ImGui::NewLine();
    ImGui::Indent(rowIndent);
    for (const char *p = row3; *p != '\0'; ++p) {
        char displayChar = (isShift) ? std::toupper(*p) : *p;
        renderKey(std::string(1, displayChar).c_str(), displayChar, keyWidth, keyHeight, inputText);
    }

    // Fourth Row, with Shift and Delete
    ImGui::NewLine();
    ImGui::Unindent(rowIndent * 2);

    if (isShift)
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

    if (ImGui::Button(m_shiftMode == ShiftMode::ShiftLock ? "SHIFT" : "Shift", ImVec2(keyWidth * 1.5f, keyHeight))) {
        m_shiftMode = static_cast<ShiftMode>((static_cast<int>(m_shiftMode) + 1) % 3);
        m_modifiers = (m_shiftMode != ShiftMode::Off) ? KDGui::KeyboardModifier::Mod_Shift : KDGui::KeyboardModifier::Mod_NoModifiers;
    }

    if (isShift)
        ImGui::PopStyleColor();

    ImGui::SameLine();
    for (const char *p = row4; *p != '\0'; ++p) {
        char displayChar = (isShift) ? std::toupper(*p) : *p;
        renderKey(std::string(1, displayChar).c_str(), displayChar, keyWidth, keyHeight, inputText);
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete", ImVec2(keyWidth * 2 + keySpacing, keyHeight)) && !inputText.empty()) {
        inputText.pop_back();
        m_pressedKey = KDGui::Key::Key_Backspace;
    }

    // Bottom Row, with space bar and enter
    auto lastRowIndent = keyWidth * 2 + keySpacing;
    ImGui::Indent(lastRowIndent);

    if (ImGui::Button("Space", ImVec2(keyWidth * 6 + keySpacing * 6, keyHeight))) {
        inputText += ' ';
        m_pressedKey = KDGui::Key::Key_Space;
    }
    ImGui::SameLine();
    if (ImGui::Button("Enter", ImVec2(keyWidth * 2 + keySpacing * 2, keyHeight))) {
        m_pressedKey = KDGui::Key::Key_Enter;
    }

    ImGui::Unindent(lastRowIndent);
}

std::optional<KDGui::Key> ImGuiKeyboard::pressedKey() const
{
    return m_pressedKey;
}

KDGui::KeyboardModifier ImGuiKeyboard::modifiers() const
{
    return m_modifiers;
}
