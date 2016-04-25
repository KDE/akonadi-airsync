cmake_minimum_required(VERSION 2.8.12)

project(airsyncdownload)

find_package(ECM 5.19 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECompilerSettings)
include(KDECMakeSettings)
include(KDEFrameworkCompilerSettings NO_POLICY_SCOPE)

add_definitions(-DTRANSLATION_DOMAIN="airsyncdownload_resource")

set(KF5_VERSION "5.19")
set(QT_REQUIRED_VERSION "5.5")
set(AKONADI_VERSION "5.2.0")
set(KMIME_LIB_VERSION "5.2.0")

# QT5 package
find_package(Qt5 ${QT_REQUIRED_VERSION} CONFIG REQUIRED Network Widgets Xml DBus)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(KF5Config ${KF5_VERSION} CONFIG REQUIRED)
find_package(KF5ConfigWidgets ${KF5_VERSION} CONFIG REQUIRED)
find_package(KF5WindowSystem ${KF5_VERSION} CONFIG REQUIRED)
find_package(KF5Wallet ${KF5_VERSION} CONFIG REQUIRED)
find_package(KF5Mime ${KMIME_LIB_VERSION} CONFIG REQUIRED)

# KdepimLibs package
find_package(KF5Akonadi ${AKONADI_VERSION} CONFIG REQUIRED)
find_package(KF5AkonadiMime ${AKONADI_VERSION} CONFIG REQUIRED)

# Xsltproc
find_program(XSLTPROC_EXECUTABLE xsltproc DOC "Path to the xsltproc executable")
mark_as_advanced(XSLTPROC_EXECUTABLE)

if(XSLTPROC_EXECUTABLE)
  set(Xsltproc_FOUND TRUE)

  # We depend on kdepimlibs, make sure it's found
  if(NOT DEFINED KF5Akonadi_DATA_DIR)
    find_package(KF5Akonadi REQUIRED)
  endif()


  # Macro to generate a D-Bus interface description from a KConfigXT file
  macro(kcfg_generate_dbus_interface _kcfg _name)
    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_name}.xml
      COMMAND ${XSLTPROC_EXECUTABLE} --stringparam interfaceName ${_name}
      ${KF5Akonadi_DATA_DIR}/kcfg2dbus.xsl
      ${_kcfg}
      > ${CMAKE_CURRENT_BINARY_DIR}/${_name}.xml
      DEPENDS ${KF5Akonadi_DATA_DIR}/kcfg2dbus.xsl
      ${_kcfg}
      )
  endmacro()
endif()

set(airsyncdownloadresource_SRCS
    AirsyncDownloadResource.cxx
    SettingsDialog.cxx
    Session.cxx
)

install(FILES airsyncdownloadresource.desktop DESTINATION "${KDE_INSTALL_DATAROOTDIR}/akonadi/agents")

ki18n_wrap_ui(airsyncdownloadresource_SRCS Settings.ui)
kconfig_add_kcfg_files(airsyncdownloadresource_SRCS settings.kcfgc)
kcfg_generate_dbus_interface(${CMAKE_CURRENT_SOURCE_DIR}/airsyncdownloadresource.kcfg org.kde.Akonadi.AirsyncDownload.Settings)
qt5_add_dbus_adaptor(airsyncdownloadresource_SRCS
  ${CMAKE_CURRENT_BINARY_DIR}/org.kde.Akonadi.AirsyncDownload.Settings.xml settings.h Settings
)

#add_executable(akonadi_airsyncdownload_resource RUN_UNINSTALLED ${airsyncdownloadresource_SRCS})
add_executable(akonadi_airsyncdownload_resource ${airsyncdownloadresource_SRCS})

target_link_libraries(akonadi_airsyncdownload_resource
  KF5::AkonadiCore
  KF5::AkonadiMime
  KF5::Mime
  KF5::AkonadiAgentBase
  KF5::AkonadiWidgets
  KF5::WindowSystem
  KF5::Wallet
  KF5::I18n
  Qt5::Network
  Qt5::Xml
  Qt5::DBus
  wbxml2
)

install(TARGETS akonadi_airsyncdownload_resource ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
