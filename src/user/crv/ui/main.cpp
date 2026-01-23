// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Config app qt entry point.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#include <dink/container.hpp>
#include <QApplication>
#include <QMessageBox>

namespace curves {

struct default_message_box_t
{
    auto exec() const noexcept -> int
    {
        return QMessageBox{QMessageBox::Information, "Curves Configuration", "Package installed successfully!"}.exec();
    }
};

static inline auto main(int argc, char* argv[]) -> int
{
    auto container = dink::Container{};

    auto app = QApplication{argc, argv};
    QApplication::setApplicationName("curves");
    QApplication::setOrganizationName("");

    return container.resolve<default_message_box_t>().exec();
}

} // namespace curves

auto main(int argc, char* argv[]) -> int
{
    return curves::main(argc, argv);
}
