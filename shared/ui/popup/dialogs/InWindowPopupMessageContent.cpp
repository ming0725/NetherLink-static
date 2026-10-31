#include "shared/ui/popup/dialogs/InWindowPopupMessageContent.h"

#include "shared/ui/PaintedLabel.h"
#include "shared/ui/popup/dialogs/InWindowPopupDialogStyle.h"
#include "shared/ui/StatefulPushButton.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

namespace InWindowPopup {

MessageContent::MessageContent(const QString& title,
                               const QString& text,
                               Button defaultButton,
                               QWidget* parent)
    : QWidget(parent)
{
    setMinimumWidth(360);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(DialogStyle::kPopupPadding,
                               DialogStyle::kPopupPadding,
                               DialogStyle::kPopupPadding,
                               DialogStyle::kPopupPadding);
    layout->setSpacing(14);

    layout->addWidget(DialogStyle::createTitleLabel(title, this));
    layout->addWidget(DialogStyle::createBodyLabel(text, this));
    layout->addSpacing(2);

    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->setContentsMargins(0, 4, 0, 0);
    buttonLayout->setSpacing(10);
    buttonLayout->addStretch();

    if (defaultButton == Button::Ok) {
        auto* okButton = DialogStyle::createButton(QStringLiteral("确认"), this);
        okButton->setPrimaryStyle();
        okButton->setFocus();
        buttonLayout->addWidget(okButton);
        layout->addLayout(buttonLayout);
        connect(okButton, &QPushButton::clicked, this, [this]() {
            if (buttonActivated) {
                buttonActivated(Button::Ok);
            }
        });
        return;
    }

    auto* noButton = DialogStyle::createButton(QStringLiteral("取消"), this);
    DialogStyle::applyDefaultButtonStyle(noButton);
    auto* yesButton = DialogStyle::createButton(QStringLiteral("确认"), this);
    yesButton->setPrimaryStyle();
    if (defaultButton == Button::No) {
        noButton->setFocus();
    } else {
        yesButton->setFocus();
    }

    buttonLayout->addWidget(noButton);
    buttonLayout->addWidget(yesButton);
    layout->addLayout(buttonLayout);

    connect(noButton, &QPushButton::clicked, this, [this]() {
        if (buttonActivated) {
            buttonActivated(Button::No);
        }
    });
    connect(yesButton, &QPushButton::clicked, this, [this]() {
        if (buttonActivated) {
            buttonActivated(Button::Yes);
        }
    });
}

} // namespace InWindowPopup
