/*
  This file is part of KDGpu.

  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include <string>
#include <optional>
#include <imgui.h>
#include <KDGui/kdgui_keys.h>

/**
 * @class ImGuiKeyboard
 * @brief A class to render a virtual keyboard using ImGui.
 *
 * The ImGuiKeyboard class provides functionality to render a virtual keyboard
 * using ImGui and capture the key presses and modifiers applied during the
 * rendering.
 */
class ImGuiKeyboard
{
public:
    /**
     * @brief Constructor for ImGuiKeyboard.
     */
    ImGuiKeyboard();

    /**
     * @brief Renders the virtual keyboard and updates the input text.
     * @param inputText The text to be updated with the key presses.
     */
    void renderKeyboard(std::string &inputText);

    /**
     * @brief Gets the key that was pressed during the last render.
     * @return An optional containing the key that was pressed, or std::nullopt if no key was pressed.
     */
    std::optional<KDGui::Key> pressedKey() const;

    /**
     * @brief Gets the keyboard modifiers applied during the last render.
     * @return The keyboard modifiers applied.
     */
    KDGui::KeyboardModifier modifiers() const;

private:
    /**
     * @brief Gets the shifted character for a given character.
     * @param c The character to be shifted.
     * @return The shifted character.
     */
    char getShiftedChar(char c);

    /**
     * @brief Renders a single key on the virtual keyboard.
     * @param label The label of the key.
     * @param outputChar The character output by the key.
     * @param keyWidth The width of the key.
     * @param keyHeight The height of the key.
     * @param inputText The text to be updated with the key press.
     */
    void renderKey(const char *label, char outputChar, float keyWidth, float keyHeight, std::string &inputText);

    /**
     * @enum ShiftMode
     * @brief Enum to represent the shift mode of the keyboard.
     */
    enum class ShiftMode {
        Off, ///< Shift is off.
        Shift, ///< Shift is on for the next key press.
        ShiftLock ///< Shift is locked on.
    };

    ShiftMode m_shiftMode; ///< The current shift mode of the keyboard.
    std::optional<KDGui::Key> m_pressedKey; ///< The key that was pressed during the last render.
    KDGui::KeyboardModifier m_modifiers; ///< The keyboard modifiers applied during the last render.
};
