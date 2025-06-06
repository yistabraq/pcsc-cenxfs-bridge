# Generated by BoostInstall.cmake for boost_concept_check-1.88.0

if(Boost_VERBOSE OR Boost_DEBUG)
  message(STATUS "Found boost_concept_check ${boost_concept_check_VERSION} at ${boost_concept_check_DIR}")
endif()

include(CMakeFindDependencyMacro)

if(NOT boost_config_FOUND)
  find_dependency(boost_config 1.88.0 EXACT HINTS "${CMAKE_CURRENT_LIST_DIR}/..")
endif()
if(NOT boost_preprocessor_FOUND)
  find_dependency(boost_preprocessor 1.88.0 EXACT HINTS "${CMAKE_CURRENT_LIST_DIR}/..")
endif()
if(NOT boost_static_assert_FOUND)
  find_dependency(boost_static_assert 1.88.0 EXACT HINTS "${CMAKE_CURRENT_LIST_DIR}/..")
endif()
if(NOT boost_type_traits_FOUND)
  find_dependency(boost_type_traits 1.88.0 EXACT HINTS "${CMAKE_CURRENT_LIST_DIR}/..")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/boost_concept_check-targets.cmake")
