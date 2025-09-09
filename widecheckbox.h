#ifndef WIDECHECKBOX_H
#define WIDECHECKBOX_H

#include <QCheckBox>

/**
 * @brief A custom QCheckBox with inherited constructors.
 *
 * This class provides a wide checkbox widget by inheriting from QCheckBox
 * and using all its constructors. It can be used in layouts where a wider
 * checkbox appearance is desired.
 */
class WideCheckBox : public QCheckBox {
    Q_OBJECT

public:
    using QCheckBox::QCheckBox; ///< Inherit all constructors from QCheckBox
};

#endif // WIDECHECKBOX_H

