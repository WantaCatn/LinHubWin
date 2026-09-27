cmake_minimum_required(VERSION 3.16)
project(LinHubWin VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
if(MSVC)
    foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${cfg} ${CMAKE_BINARY_DIR})
    endforeach()
endif()

find_package(Qt5 QUIET COMPONENTS Core Widgets Sql)
if(Qt5_FOUND)
    set(QT_VERSION_MAJOR 5)
else()
    find_package(Qt6 REQUIRED COMPONENTS Core Widgets Sql)
    set(QT_VERSION_MAJOR 6)
endif()

set(LINHUB_SOURCES
    src/main.cpp
    src/app/application.cpp
    src/app/appsettings.cpp
    src/core/session.cpp
    src/storage/databasemanager.cpp
    src/storage/sessionrepository.cpp
    src/storage/historyrepository.cpp
    src/storage/commandrepository.cpp
    src/storage/transferrepository.cpp
    src/pty/ptyprocess.cpp
    src/pty/winptyreader.cpp
    src/terminal/screen.cpp
    src/terminal/parser.cpp
    src/terminal/terminalwidget.cpp
    src/terminal/colorscheme.cpp
    src/net/sessionlauncher.cpp
    src/net/askpass.cpp
    src/net/sftpclient.cpp
    src/logging/sessionlogger.cpp
    src/util/crypto.cpp
    src/util/sshconfigimporter.cpp
    src/ui/mainwindow.cpp
    src/ui/sessiontreewidget.cpp
    src/ui/sessioneditdialog.cpp
    src/ui/quickconnectbar.cpp
    src/ui/composebar.cpp
    src/ui/historypanel.cpp
    src/ui/settingsdialog.cpp
    src/ui/sftpwidget.cpp
    src/ui/tabterminal.cpp
    src/ui/sessionpane.cpp
    src/ui/icons.cpp
    src/ui/startpage.cpp
    src/ui/findbar.cpp
    src/ui/authpromptdialog.cpp
    resources/linhub.qrc
)

if(WIN32)
    list(APPEND LINHUB_SOURCES resources/linhub.rc)
endif()

if(WIN32)
    add_executable(linhub WIN32 ${LINHUB_SOURCES})
else()
    add_executable(linhub ${LINHUB_SOURCES})
endif()

target_include_directories(linhub PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_definitions(linhub PRIVATE
    LINHUB_VERSION="${PROJECT_VERSION}"
    QT_NO_CAST_FROM_ASCII
    QT_NO_NARROWING_CONVERSIONS_IN_CONNECT
)

target_link_libraries(linhub PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Sql
)

if(WIN32)
    target_link_libraries(linhub PRIVATE kernel32 user32)
    # Win7 x64 起可用；Win10 1809+ 走 ConPTY，更早系统回退到匿名管道
    target_compile_definitions(linhub PRIVATE
        WINVER=0x0601
        _WIN32_WINNT=0x0601
        NTDDI_VERSION=0x06010000
    )
    if(MINGW)
        target_link_options(linhub PRIVATE -static-libgcc -static-libstdc++)
    endif()
endif()

if(MSVC)
    target_compile_options(linhub PRIVATE /utf-8 /W4)
else()
    target_compile_options(linhub PRIVATE -Wall -Wextra)
endif()

add_executable(linhub-askpass src/net/askpass_main.cpp src/net/askpass.cpp)
target_include_directories(linhub-askpass PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(linhub-askpass PRIVATE Qt${QT_VERSION_MAJOR}::Core)
if(WIN32 AND MINGW)
    target_link_options(linhub-askpass PRIVATE -static-libgcc -static-libstdc++)
endif()

include(GNUInstallDirs)
install(TARGETS linhub linhub-askpass RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(WIN32)
    install(FILES resources/icons/linhub.ico DESTINATION ${CMAKE_INSTALL_BINDIR})
    set(OPENSSH_DIR ${CMAKE_SOURCE_DIR}/third_party/openssh)
    if(EXISTS ${OPENSSH_DIR}/ssh.exe)
        file(GLOB OPENSSH_FILES
            ${OPENSSH_DIR}/ssh.exe
            ${OPENSSH_DIR}/scp.exe
            ${OPENSSH_DIR}/sftp.exe
            ${OPENSSH_DIR}/ssh-add.exe
            ${OPENSSH_DIR}/ssh-agent.exe
            ${OPENSSH_DIR}/ssh-keygen.exe
            ${OPENSSH_DIR}/ssh-keyscan.exe
            ${OPENSSH_DIR}/libcrypto.dll
            ${OPENSSH_DIR}/README.txt
        )
        add_custom_command(TARGET linhub POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${OPENSSH_FILES}
                $<TARGET_FILE_DIR:linhub>
            COMMENT "Copy bundled OpenSSH next to linhub.exe"
        )
        install(FILES ${OPENSSH_FILES} DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
endif()
