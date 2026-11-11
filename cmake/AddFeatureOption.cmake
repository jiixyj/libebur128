# SPDX-FileCopyrightText: 2026 thibaud https://thibaud.space/
# SPDX-License-Identifier: CC0-1.0
include(FeatureSummary)

function(add_feature_option
    _variable_name
    _option_description
    _option_default
)
    if(NOT DEFINED default)
        set(default OFF)
    endif()

    # option(<variable> "<help_text>" [value])
    option(${_variable_name} ${_option_description} ${_option_default})

    # add_feature_info(<name> <enabled> <description>)
    set(_option_value ${${_variable_name}})
    add_feature_info(${_variable_name} ${_option_value} ${_option_description})
endfunction()
