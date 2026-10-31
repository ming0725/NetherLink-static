#pragma once

#include <QLineEdit>
#include <QString>

class QWidget;

namespace InWindowPopup {

enum class Button {
    NoButton,
    Yes,
    No,
    Ok,
    Cancel
};

Button question(QWidget* parent,
                const QString& title,
                const QString& text,
                Button defaultButton = Button::No,
                bool dismissOnOutsideClick = true,
                bool dismissOnEscape = true);

QString getText(QWidget* parent,
                const QString& title,
                const QString& label,
                QLineEdit::EchoMode echo = QLineEdit::Normal,
                const QString& text = QString(),
                bool* accepted = nullptr);

} // namespace InWindowPopup
