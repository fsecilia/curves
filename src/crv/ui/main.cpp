// SPDX-License-Identifier: MIT
/*!
    \file
    \brief config app qt entry point

    \copyright Copyright (C) 2026 Frank Secilia
*/

#include <dink/container.hpp>
#include <QApplication>
#include <QMessageBox>
#include <cstdlib>
#include <iostream>
#include <utility>

namespace curves {

struct message_box_params_t
{
    QMessageBox::Icon icon  = QMessageBox::Information;
    QString           title = "Curves Configuration";
    QString           text  = "Package installed successfully!";
};

struct default_message_box_t
{
    message_box_params_t message_box_params;

    explicit default_message_box_t(message_box_params_t message_box_params) noexcept
        : message_box_params{std::move(message_box_params)}
    {}

    auto exec() const noexcept -> int
    {
        return QMessageBox{QMessageBox::Information, "Curves Configuration", "Package installed successfully!"}.exec();
    }
};

auto run_application(int argc, char* argv[]) -> int
{
    auto container = dink::Container{};

    auto app = QApplication{argc, argv};
    QApplication::setApplicationName("curves");
    QApplication::setOrganizationName("");

    // This isn't how you'd normally use a container, but it proves that it works.
    return container.resolve<default_message_box_t>().exec();
}

auto run_oneshot_config(int, char*[]) -> int
{
    std::cout << "configuration restored successfully!" << std::endl;
    return EXIT_SUCCESS;
}

auto main(int argc, char* argv[]) -> int
{
    auto const restore_config_option = std::string_view{"--restore-config"};

    for (auto i = 1; i < argc; ++i)
    {
        if (argv[i] == restore_config_option) return run_oneshot_config(argc, argv);
    }

    return run_application(argc, argv);
}

} // namespace curves

auto main(int argc, char* argv[]) -> int
{
    return curves::main(argc, argv);
}
