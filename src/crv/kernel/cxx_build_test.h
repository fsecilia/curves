// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*!
    \file
    \brief temporary test module to make sure c++ builds freestanding

    There won't be an actual c++ kernel module until we get closer to the pipeline. For now, this module artificially
    refers to c++ headers and instantiates templates, just to make sure everything compiles.

    \copyright Copyright (C) 2026 Frank Secilia
*/

int reference_cxx(void);
