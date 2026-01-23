// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Config app qt entry point.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#include <QApplication>
#include <QMessageBox>

namespace curves {

static inline auto main(int argc, char* argv[]) -> int
{
    auto app = QApplication{argc, argv};
    QApplication::setApplicationName("curves");
    QApplication::setOrganizationName("");

    return QMessageBox{QMessageBox::Information, "Curves Configuration", "Package installed successfully!"}.exec();
}

} // namespace curves

auto main(int argc, char* argv[]) -> int
{
    return curves::main(argc, argv);
}
