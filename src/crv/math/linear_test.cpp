// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "linear.hpp"
#include <crv/test/test.hpp>

namespace crv::math {
namespace {

// ====================================================================================================================
// integer tests
// ====================================================================================================================

namespace int_tests {

using element_t                                  = int_t;
using scalar_t                                   = scalar_t<element_t>;
template <int_t size> using vector_t             = vector_t<element_t, size>;
template <int_t rows, int_t cols> using matrix_t = matrix_t<element_t, rows, cols>;

// clang-format off

// --------------------------------------------------------------------------------------------------------------------
// construction
// --------------------------------------------------------------------------------------------------------------------

// scalar
static_assert(scalar_t{0xa0} == 0xa0);

// vector
static_assert(vector_t<1>{0xa0}[0] == 0xa0);
static_assert(vector_t<2>{0xa0, 0xa1}[0] == 0xa0);
static_assert(vector_t<2>{0xa0, 0xa1}[1] == 0xa1);

// matrix
static_assert(matrix_t<1, 1>{{{{0xa00}}}}[0] == vector_t<1>{0xa00});
static_assert(matrix_t<1, 2>{{{{0xa00, 0xa01}}}}[0] == vector_t<2>{0xa00, 0xa01});
static_assert(matrix_t<2, 1>{{{{0xa00}, {0xa10}}}}[0] == vector_t<1>{0xa00});
static_assert(matrix_t<2, 1>{{{{0xa00}, {0xa10}}}}[1] == vector_t<1>{0xa10});
static_assert(matrix_t<2, 2>{{{{0xa00, 0xa01}, {0xa10, 0xa11}}}}[0] == vector_t<2>{0xa00, 0xa01});
static_assert(matrix_t<2, 2>{{{{0xa00, 0xa01}, {0xa10, 0xa11}}}}[1] == vector_t<2>{0xa10, 0xa11});

// 2x3x5 matrix
static_assert(
    linear_t<element_t, 2, 3, 5>
    {{{
        {{{
            {{0xa000, 0xa001, 0xa002, 0xa003, 0xa004}},
            {{0xa010, 0xa011, 0xa012, 0xa013, 0xa014}},
            {{0xa020, 0xa021, 0xa022, 0xa023, 0xa024}},
        }}},
        {{{
            {{0xa100, 0xa101, 0xa102, 0xa103, 0xa104}},
            {{0xa110, 0xa111, 0xa112, 0xa113, 0xa114}},
            {{0xa120, 0xa121, 0xa122, 0xa123, 0xa124}},
        }}},
    }}}[0]
    ==
    matrix_t<3, 5>
    {{{
        {{0xa000, 0xa001, 0xa002, 0xa003, 0xa004}},
        {{0xa010, 0xa011, 0xa012, 0xa013, 0xa014}},
        {{0xa020, 0xa021, 0xa022, 0xa023, 0xa024}},
    }}}
);

static_assert(
    linear_t<element_t, 2, 3, 5>
    {{{
        {{{
            {{0xa000, 0xa001, 0xa002, 0xa003, 0xa004}},
            {{0xa010, 0xa011, 0xa012, 0xa013, 0xa014}},
            {{0xa020, 0xa021, 0xa022, 0xa023, 0xa024}},
        }}},
        {{{
            {{0xa100, 0xa101, 0xa102, 0xa103, 0xa104}},
            {{0xa110, 0xa111, 0xa112, 0xa113, 0xa114}},
            {{0xa120, 0xa121, 0xa122, 0xa123, 0xa124}},
        }}},
    }}}[1]
    ==
    matrix_t<3, 5>
    {{{
        {{0xa100, 0xa101, 0xa102, 0xa103, 0xa104}},
        {{0xa110, 0xa111, 0xa112, 0xa113, 0xa114}},
        {{0xa120, 0xa121, 0xa122, 0xa123, 0xa124}},
    }}}
);

// --------------------------------------------------------------------------------------------------------------------
// unary negation
// --------------------------------------------------------------------------------------------------------------------

static_assert(-scalar_t{0xa0} == -0xa0);

static_assert(-vector_t<1>{0xa0}[0] == -0xa0);
static_assert(-vector_t<2>{0xa0, 0xa1}[0] == -0xa0);
static_assert(-vector_t<2>{0xa0, 0xa1}[1] == -0xa1);

static_assert(-matrix_t<1, 1>{{{{0xa00}}}}[0] == -vector_t<1>{0xa00});
static_assert(-matrix_t<1, 2>{{{{0xa00, 0xa01}}}}[0] == -vector_t<2>{0xa00, 0xa01});
static_assert(-matrix_t<2, 1>{{{{0xa00}, {0xa10}}}}[0] == -vector_t<1>{0xa00});
static_assert(-matrix_t<2, 1>{{{{0xa00}, {0xa10}}}}[1] == -vector_t<1>{0xa10});
static_assert(-matrix_t<2, 2>{{{{0xa00, 0xa01}, {0xa10, 0xa11}}}}[0] == -vector_t<2>{0xa00, 0xa01});
static_assert(-matrix_t<2, 2>{{{{0xa00, 0xa01}, {0xa10, 0xa11}}}}[1] == -vector_t<2>{0xa10, 0xa11});

// --------------------------------------------------------------------------------------------------------------------
// addition, same + same
// --------------------------------------------------------------------------------------------------------------------

static_assert(vector_t<1>{0xa0} + vector_t<1>{0xb0} == vector_t<1>{0xa0 + 0xb0});
static_assert(vector_t<2>{0xa0, 0xa1} + vector_t<2>{0xb0, 0xb1} == vector_t<2>{0xa0 + 0xb0, 0xa1 + 0xb1});
static_assert(matrix_t<1, 1>{{{{0xa00}}}} + matrix_t<1, 1>{{{{0xb00}}}} == matrix_t<1, 1>{{{{0xa00 + 0xb00}}}});
static_assert
(
    matrix_t<1, 2>{{{{0xa00, 0xa01}}}} + matrix_t<1, 2>{{{{0xb00, 0xb01}}}}
    ==
    matrix_t<1, 2>{{{{0xa00 + 0xb00, 0xa01 + 0xb01}}}}
);

static_assert
(
    matrix_t<2, 1>{{{{0xa00}, {0xa10}}}} + matrix_t<2, 1>{{{{0xb00}, {0xb10}}}}
    ==
    matrix_t<2, 1>{{{{0xa00 + 0xb00}, {0xa10 + 0xb10}}}}
);

static_assert
(
    matrix_t<2, 2>{{{{0xa00, 0xa01}, {0xa10, 0xa11}}}} + matrix_t<2, 2>{{{{0xb00, 0xb01}, {0xb10, 0xb11}}}}
    ==
    matrix_t<2, 2>{{{{0xa00 + 0xb00, 0xa01 + 0xb01}, {0xa10 + 0xb10, 0xa11 + 0xb11}}}}
);

// --------------------------------------------------------------------------------------------------------------------
// addition, matrices and vectors
// --------------------------------------------------------------------------------------------------------------------

static_assert(matrix_t<1, 1>{{{{0xa00}}}} + vector_t<1>{0xb0} == matrix_t<1, 1>{{{{0xa00 + 0xb0}}}});
static_assert
(
    matrix_t<1, 2>{{{{0xa00, 0xa01}}}} + vector_t<2>{0xb0, 0xb1}
    ==
    matrix_t<1, 2>{{{{0xa00 + 0xb0, 0xa01 + 0xb1}}}}
);

static_assert
(
    matrix_t<2, 1>{{{{0xa00}, {0xa10}}}} + vector_t<1>{0xb0}
    ==
    matrix_t<2, 1>{{{{0xa00 + 0xb0}, {0xa10 + 0xb0}}}}
);

static_assert
(
    matrix_t<2, 2>{{{{0xa00, 0xa01}, {0xa10, 0xa11}}}} + vector_t<2>{0xb0, 0xb1}
    ==
    matrix_t<2, 2>{{{{0xa00 + 0xb0, 0xa01 + 0xb1}, {0xa10 + 0xb0, 0xa11 + 0xb1}}}}
);

static_assert(vector_t<1>{0xa0} + matrix_t<1, 1>{{{{0xb00}}}}== matrix_t<1, 1>{{{{0xa0 + 0xb00}}}});
static_assert
(
    vector_t<2>{0xa0, 0xa1} + matrix_t<1, 2>{{{{0xb00, 0xb01}}}}
    ==
    matrix_t<1, 2>{{{{0xa0 + 0xb00, 0xa1 + 0xb01}}}}
);

static_assert
(
    vector_t<1>{0xa0} + matrix_t<2, 1>{{{{0xb00}, {0xb10}}}}
    ==
    matrix_t<2, 1>{{{{0xa0 + 0xb00}, {0xa0 + 0xb10}}}}
);

static_assert
(
    vector_t<2>{0xa0, 0xa1} + matrix_t<2, 2>{{{{0xb00, 0xb01}, {0xb10, 0xb11}}}}
    ==
    matrix_t<2, 2>{{{{0xa0 + 0xb00, 0xa1 + 0xb01}, {0xa0 + 0xb10, 0xa1 + 0xb11}}}}
);

// --------------------------------------------------------------------------------------------------------------------
// addition, scalars and vectors or matrices
// --------------------------------------------------------------------------------------------------------------------

static_assert(0xa + vector_t<1>{0xb0} == vector_t<1>{0xa + 0xb0});
static_assert(0xa + vector_t<2>{0xb0, 0xb1} == vector_t<2>{0xa + 0xb0, 0xa + 0xb1});
static_assert(0xa + matrix_t<1, 1>{{{{0xb00}}}} == matrix_t<1, 1>{{{{0xa + 0xb00}}}});
static_assert(0xa + matrix_t<1, 2>{{{{0xb00, 0xb01}}}} == matrix_t<1, 2>{{{{0xa + 0xb00, 0xa + 0xb01}}}});

static_assert(scalar_t{0xa} + vector_t<1>{0xb0} == vector_t<1>{0xa + 0xb0});
static_assert(scalar_t{0xa} + vector_t<2>{0xb0, 0xb1} == vector_t<2>{0xa + 0xb0, 0xa + 0xb1});
static_assert(scalar_t{0xa} + matrix_t<1, 1>{{{{0xb00}}}} == matrix_t<1, 1>{{{{0xa + 0xb00}}}});
static_assert(scalar_t{0xa} + matrix_t<1, 2>{{{{0xb00, 0xb01}}}} == matrix_t<1, 2>{{{{0xa + 0xb00, 0xa + 0xb01}}}});
static_assert
(
    scalar_t{0xa} + matrix_t<2, 1>{{{{0xb00}, {0xb10}}}}
    ==
    matrix_t<2, 1>{{{{0xa + 0xb00}, {0xa + 0xb10}}}}
);

static_assert
(
    scalar_t{0xa} + matrix_t<2, 2>{{{{0xb00, 0xb01}, {0xb10, 0xb11}}}}
    ==
    matrix_t<2, 2>{{{{0xa + 0xb00, 0xa + 0xb01}, {0xa + 0xb10, 0xa + 0xb11}}}}
);

static_assert(vector_t<1>{0xa0} + 0xb == vector_t<1>{0xa0 + 0xb});
static_assert(vector_t<1>{0xa0} + scalar_t{0xb} == vector_t<1>{0xa0 + 0xb});
static_assert(vector_t<2>{0xa0, 0xa1} + scalar_t{0xb} == vector_t<2>{0xa0 + 0xb, 0xa1 + 0xb});
static_assert(matrix_t<1, 1>{{{{0xa00}}}} + scalar_t{0xb} == matrix_t<1, 1>{{{{0xa00 + 0xb}}}});
static_assert(matrix_t<1, 2>{{{{0xa00, 0xa01}}}} + scalar_t{0xb} == matrix_t<1, 2>{{{{0xa00 + 0xb, 0xa01 + 0xb}}}});
static_assert
(
    matrix_t<2, 1>{{{{0xa00}, {0xa10}}}} + scalar_t{0xb}
    ==
    matrix_t<2, 1>{{{{0xa00 + 0xb}, {0xa10 + 0xb}}}}
);

static_assert
(
    matrix_t<2, 2>{{{{0xa00, 0xa01}, {0xa10, 0xa11}}}} + scalar_t{0xb}
    ==
    matrix_t<2, 2>{{{{0xa00 + 0xb, 0xa01 + 0xb}, {0xa10 + 0xb, 0xa11 + 0xb}}}}
);

// --------------------------------------------------------------------------------------------------------------------
// subtraction, same - same
// --------------------------------------------------------------------------------------------------------------------

static_assert(vector_t<1>{0xa0} - vector_t<1>{0xb0} == vector_t<1>{0xa0 - 0xb0});
static_assert(vector_t<2>{0xa0, 0xa1} - vector_t<2>{0xb0, 0xb1} == vector_t<2>{0xa0 - 0xb0, 0xa1 - 0xb1});
static_assert(matrix_t<1, 1>{{{{0xa00}}}} - matrix_t<1, 1>{{{{0xb00}}}} == matrix_t<1, 1>{{{{0xa00 - 0xb00}}}});
static_assert
(
    matrix_t<1, 2>{{{{0xa00, 0xa01}}}} - matrix_t<1, 2>{{{{0xb00, 0xb01}}}}
    ==
    matrix_t<1, 2>{{{{0xa00 - 0xb00, 0xa01 - 0xb01}}}}
);

static_assert
(
    matrix_t<2, 1>{{{{0xa00}, {0xa10}}}} - matrix_t<2, 1>{{{{0xb00}, {0xb10}}}}
    ==
    matrix_t<2, 1>{{{{0xa00 - 0xb00}, {0xa10 - 0xb10}}}}
);

static_assert
(
    matrix_t<2, 2>{{{{0xa00, 0xa01}, {0xa10, 0xa11}}}} - matrix_t<2, 2>{{{{0xb00, 0xb01}, {0xb10, 0xb11}}}}
    ==
    matrix_t<2, 2>{{{{0xa00 - 0xb00, 0xa01 - 0xb01}, {0xa10 - 0xb10, 0xa11 - 0xb11}}}}
);

// --------------------------------------------------------------------------------------------------------------------
// subtraction, matrices and vectors
// --------------------------------------------------------------------------------------------------------------------

static_assert(matrix_t<1, 1>{{{{0xa00}}}} - vector_t<1>{0xb0} == matrix_t<1, 1>{{{{0xa00 - 0xb0}}}});
static_assert
(
    matrix_t<1, 2>{{{{0xa00, 0xa01}}}} - vector_t<2>{0xb0, 0xb1}
    ==
    matrix_t<1, 2>{{{{0xa00 - 0xb0, 0xa01 - 0xb1}}}}
);

static_assert
(
    matrix_t<2, 1>{{{{0xa00}, {0xa10}}}} - vector_t<1>{0xb0}
    ==
    matrix_t<2, 1>{{{{0xa00 - 0xb0}, {0xa10 - 0xb0}}}}
);

static_assert
(
    matrix_t<2, 2>{{{{0xa00, 0xa01}, {0xa10, 0xa11}}}} - vector_t<2>{0xb0, 0xb1}
    ==
    matrix_t<2, 2>{{{{0xa00 - 0xb0, 0xa01 - 0xb1}, {0xa10 - 0xb0, 0xa11 - 0xb1}}}}
);

static_assert(vector_t<1>{0xa0} - matrix_t<1, 1>{{{{0xb00}}}}== matrix_t<1, 1>{{{{0xa0 - 0xb00}}}});
static_assert
(
    vector_t<2>{0xa0, 0xa1} - matrix_t<1, 2>{{{{0xb00, 0xb01}}}}
    ==
    matrix_t<1, 2>{{{{0xa0 - 0xb00, 0xa1 - 0xb01}}}}
);

static_assert
(
    vector_t<1>{0xa0} - matrix_t<2, 1>{{{{0xb00}, {0xb10}}}}
    ==
    matrix_t<2, 1>{{{{0xa0 - 0xb00}, {0xa0 - 0xb10}}}}
);

static_assert
(
    vector_t<2>{0xa0, 0xa1} - matrix_t<2, 2>{{{{0xb00, 0xb01}, {0xb10, 0xb11}}}}
    ==
    matrix_t<2, 2>{{{{0xa0 - 0xb00, 0xa1 - 0xb01}, {0xa0 - 0xb10, 0xa1 - 0xb11}}}}
);

// --------------------------------------------------------------------------------------------------------------------
// subtraction, scalars and vectors or matrices
// --------------------------------------------------------------------------------------------------------------------

static_assert(0xa - vector_t<1>{0xb0} == vector_t<1>{0xa - 0xb0});
static_assert(scalar_t{0xa} - vector_t<1>{0xb0} == vector_t<1>{0xa - 0xb0});
static_assert(scalar_t{0xa} - vector_t<2>{0xb0, 0xb1} == vector_t<2>{0xa - 0xb0, 0xa - 0xb1});
static_assert(scalar_t{0xa} - matrix_t<1, 1>{{{{0xb00}}}} == matrix_t<1, 1>{{{{0xa - 0xb00}}}});
static_assert(scalar_t{0xa} - matrix_t<1, 2>{{{{0xb00, 0xb01}}}} == matrix_t<1, 2>{{{{0xa - 0xb00, 0xa - 0xb01}}}});
static_assert
(
    scalar_t{0xa} - matrix_t<2, 1>{{{{0xb00}, {0xb10}}}}
    ==
    matrix_t<2, 1>{{{{0xa - 0xb00}, {0xa - 0xb10}}}}
);

static_assert
(
    scalar_t{0xa} - matrix_t<2, 2>{{{{0xb00, 0xb01}, {0xb10, 0xb11}}}}
    ==
    matrix_t<2, 2>{{{{0xa - 0xb00, 0xa - 0xb01}, {0xa - 0xb10, 0xa - 0xb11}}}}
);

static_assert(vector_t<1>{0xa0} - 0xb == vector_t<1>{0xa0 - 0xb});
static_assert(vector_t<1>{0xa0} - scalar_t{0xb} == vector_t<1>{0xa0 - 0xb});
static_assert(vector_t<2>{0xa0, 0xa1} - scalar_t{0xb} == vector_t<2>{0xa0 - 0xb, 0xa1 - 0xb});
static_assert(matrix_t<1, 1>{{{{0xa00}}}} - scalar_t{0xb} == matrix_t<1, 1>{{{{0xa00 - 0xb}}}});
static_assert(matrix_t<1, 2>{{{{0xa00, 0xa01}}}} - scalar_t{0xb} == matrix_t<1, 2>{{{{0xa00 - 0xb, 0xa01 - 0xb}}}});
static_assert
(
    matrix_t<2, 1>{{{{0xa00}, {0xa10}}}} - scalar_t{0xb}
    ==
    matrix_t<2, 1>{{{{0xa00 - 0xb}, {0xa10 - 0xb}}}}
);

static_assert
(
    matrix_t<2, 2>{{{{0xa00, 0xa01}, {0xa10, 0xa11}}}} - scalar_t{0xb}
    ==
    matrix_t<2, 2>{{{{0xa00 - 0xb, 0xa01 - 0xb}, {0xa10 - 0xb, 0xa11 - 0xb}}}}
);

// --------------------------------------------------------------------------------------------------------------------
// scalar multiplication
// --------------------------------------------------------------------------------------------------------------------

static_assert(0xa*vector_t<1>{0xb0} == vector_t<1>{0xa*0xb0});
static_assert(scalar_t{0xa}*vector_t<1>{0xb0} == vector_t<1>{0xa*0xb0});
static_assert(scalar_t{0xa}*vector_t<2>{0xb0, 0xb1} == vector_t<2>{0xa*0xb0, 0xa*0xb1});
static_assert(scalar_t{0xa}*matrix_t<1, 1>{{{{0xb00}}}} == matrix_t<1, 1>{{{{0xa*0xb00}}}});
static_assert(scalar_t{0xa}*matrix_t<1, 2>{{{{0xb00, 0xb01}}}} == matrix_t<1, 2>{{{{0xa*0xb00, 0xa*0xb01}}}});
static_assert(scalar_t{0xa}*matrix_t<2, 1>{{{{0xb00}, {0xb10}}}} == matrix_t<2, 1>{{{{0xa*0xb00}, {0xa*0xb10}}}});
static_assert
(
    scalar_t{0xa}*matrix_t<2, 2>{{{{0xb00, 0xb01}, {0xb10, 0xb11}}}}
    ==
    matrix_t<2, 2>{{{{0xa*0xb00, 0xa*0xb01}, {0xa*0xb10, 0xa*0xb11}}}}
);

// --------------------------------------------------------------------------------------------------------------------
// vector multiplication
// --------------------------------------------------------------------------------------------------------------------

static_assert(vector_t<1>{0xa0}*0xb == vector_t<1>{0xa0*0xb});
static_assert(vector_t<1>{0xa0}*scalar_t{0xb} == vector_t<1>{0xa0*0xb});
static_assert(vector_t<1>{0xa0}*vector_t<1>{0xb0} == vector_t<1>{0xa0*0xb0});
static_assert(vector_t<1>{0xa0}*matrix_t<1, 1>{{{{0xb00}}}} == vector_t<1>{0xa0*0xb00});
static_assert(vector_t<1>{0xa0}*matrix_t<1, 2>{{{{0xb00, 0xb01}}}} == vector_t<2>{0xa0*0xb00, 0xa0*0xb01});

static_assert(vector_t<2>{0xa0, 0xa1}*scalar_t{0xb} == vector_t<2>{0xa0*0xb, 0xa1*0xb});
static_assert(vector_t<2>{0xa0, 0xa1}*vector_t<2>{0xb0, 0xb1} == vector_t<2>{0xa0*0xb0, 0xa1*0xb1});
static_assert(vector_t<2>{0xa0, 0xa1}*matrix_t<2, 1>{{{{0xb00}, {0xb10}}}} == vector_t<1>{0xa0*0xb00 + 0xa1*0xb10});
static_assert
(
    vector_t<2>{0xa0, 0xa1}*matrix_t<2, 2>{{{{0xb00, 0xb01}, {0xb10, 0xb11}}}}
    ==
    vector_t<2>{0xa0*0xb00 + 0xa1*0xb10, 0xa0*0xb01 + 0xa1*0xb11}
);

// --------------------------------------------------------------------------------------------------------------------
// matrix multiplication
// --------------------------------------------------------------------------------------------------------------------

static_assert(matrix_t<1, 1>{{{{0xa00}}}}*0xb == matrix_t<1, 1>{{{{0xa00*0xb}}}});
static_assert(matrix_t<1, 1>{{{{0xa00}}}}*scalar_t{0xb} == matrix_t<1, 1>{{{{0xa00*0xb}}}});
static_assert(matrix_t<1, 1>{{{{0xa00}}}}*vector_t<1>{0xb0} == vector_t<1>{0xa00*0xb0});
static_assert(matrix_t<1, 1>{{{{0xa00}}}}*matrix_t<1, 1>{{{{0xb00}}}} ==  matrix_t<1, 1>{{{{0xa00*0xb00}}}});
static_assert
(
    matrix_t<1, 1>{{{{0xa00}}}}*matrix_t<1, 2>{{{{0xb00, 0xb01}}}}
    ==
    matrix_t<1, 2>{{{{0xa00*0xb00, 0xa00*0xb01}}}}
);

static_assert(matrix_t<1, 2>{{{{0xa00, 0xa01}}}}*0xb == matrix_t<1, 2>{{{{0xa00*0xb, 0xa01*0xb}}}});
static_assert(matrix_t<1, 2>{{{{0xa00, 0xa01}}}}*scalar_t{0xb} == matrix_t<1, 2>{{{{0xa00*0xb, 0xa01*0xb}}}});
static_assert(matrix_t<1, 2>{{{{0xa00, 0xa01}}}}*vector_t<2>{0xb0, 0xb1} == vector_t<1>{0xa00*0xb0 + 0xa01*0xb1});
static_assert
(
    matrix_t<1, 2>{{{{0xa00, 0xa01}}}}*matrix_t<2, 1>{{{{0xb00}, {0xb10}}}}
    ==
    matrix_t<1, 1>{{{{0xa00*0xb00 + 0xa01*0xb10}}}}
);

static_assert
(
    matrix_t<1, 2>{{{{0xa00, 0xa01}}}}*matrix_t<2, 2>{{{{0xb00, 0xb01}, {0xb10, 0xb11}}}}
    ==
    matrix_t<1, 2>{{{{0xa00*0xb00 + 0xa01*0xb10, 0xa00*0xb01 + 0xa01*0xb11}}}}
);

static_assert
(
    matrix_t<2, 1>{{{{0xa00}, {0xa10}}}}*0xb
    ==
    matrix_t<2, 1>{{{{0xa00*0xb}, {0xa10*0xb}}}}
);

static_assert
(
    matrix_t<2, 1>{{{{0xa00}, {0xa10}}}}*scalar_t{0xb}
    ==
    matrix_t<2, 1>{{{{0xa00*0xb}, {0xa10*0xb}}}}
);

static_assert(matrix_t<2, 1>{{{{0xa00}, {0xa10}}}}*vector_t<1>{0xb0} == vector_t<2>{0xa00*0xb0, 0xa10*0xb0});
static_assert
(
    matrix_t<2, 1>{{{{0xa00}, {0xa10}}}}*matrix_t<1, 1>{{{{0xb00}}}}
    ==
    matrix_t<2, 1>{{{{0xa00*0xb00}, {0xa10*0xb00}}}}
);

static_assert
(
    matrix_t<2, 1>{{{{0xa00}, {0xa10}}}}*matrix_t<1, 2>{{{{0xb00, 0xb01}}}}
    ==
    matrix_t<2, 2>{{{{0xa00*0xb00, 0xa00*0xb01}, {0xa10*0xb00, 0xa10*0xb01}}}}
);

static_assert
(
    matrix_t<2, 2>{{{{0xa00, 0xa01}, {0xa10, 0xa11}}}}*0xb
    ==
    matrix_t<2, 2>{{{{0xa00*0xb, 0xa01*0xb}, {0xa10*0xb, 0xa11*0xb}}}}
);

static_assert
(
    matrix_t<2, 2>{{{{0xa00, 0xa01}, {0xa10, 0xa11}}}}*scalar_t{0xb}
    ==
    matrix_t<2, 2>{{{{0xa00*0xb, 0xa01*0xb}, {0xa10*0xb, 0xa11*0xb}}}}
);

static_assert
(
    matrix_t<2, 2>{{{{0xa00, 0xa01}, {0xa10, 0xa11}}}}*vector_t<2>{0xb0, 0xb1}
    ==
    vector_t<2>{0xa00*0xb0 + 0xa01*0xb1, 0xa10*0xb0 + 0xa11*0xb1}
);

static_assert
(
    matrix_t<2, 2>{{{{0xa00, 0xa01}, {0xa10, 0xa11}}}}*matrix_t<2, 1>{{{{0xb00}, {0xb10}}}}
    ==
    matrix_t<2, 1>{{{{0xa00*0xb00 + 0xa01*0xb10}, {0xa10*0xb00 + 0xa11*0xb10}}}}
);

static_assert
(
    matrix_t<2, 2>{{{{0xa00, 0xa01}, {0xa10, 0xa11}}}}*matrix_t<2, 2>{{{{0xb00, 0xb01}, {0xb10, 0xb11}}}}
    ==
    matrix_t<2, 2>{{{
        {0xa00*0xb00 + 0xa01*0xb10, 0xa00*0xb01 + 0xa01*0xb11},
        {0xa10*0xb00 + 0xa11*0xb10, 0xa10*0xb01 + 0xa11*0xb11},
    }}}
);

// --------------------------------------------------------------------------------------------------------------------
// 4x4 multiplication
// --------------------------------------------------------------------------------------------------------------------

static_assert
(
    matrix_t<4, 4>{{{
        {0xa00, 0xa01, 0xa02, 0xa03},
        {0xa10, 0xa11, 0xa12, 0xa13},
        {0xa20, 0xa21, 0xa22, 0xa23},
        {0xa30, 0xa31, 0xa32, 0xa33},
    }}}
    *
    matrix_t<4, 4>{{{
        {0xb00, 0xb01, 0xb02, 0xb03},
        {0xb10, 0xb11, 0xb12, 0xb13},
        {0xb20, 0xb21, 0xb22, 0xb23},
        {0xb30, 0xb31, 0xb32, 0xb33},
    }}}
    ==
    matrix_t<4, 4>{{{
        {
            0xa00*0xb00 + 0xa01*0xb10 + 0xa02*0xb20 + 0xa03*0xb30,
            0xa00*0xb01 + 0xa01*0xb11 + 0xa02*0xb21 + 0xa03*0xb31,
            0xa00*0xb02 + 0xa01*0xb12 + 0xa02*0xb22 + 0xa03*0xb32,
            0xa00*0xb03 + 0xa01*0xb13 + 0xa02*0xb23 + 0xa03*0xb33,
        },
        {
            0xa10*0xb00 + 0xa11*0xb10 + 0xa12*0xb20 + 0xa13*0xb30,
            0xa10*0xb01 + 0xa11*0xb11 + 0xa12*0xb21 + 0xa13*0xb31,
            0xa10*0xb02 + 0xa11*0xb12 + 0xa12*0xb22 + 0xa13*0xb32,
            0xa10*0xb03 + 0xa11*0xb13 + 0xa12*0xb23 + 0xa13*0xb33,
        },
        {
            0xa20*0xb00 + 0xa21*0xb10 + 0xa22*0xb20 + 0xa23*0xb30,
            0xa20*0xb01 + 0xa21*0xb11 + 0xa22*0xb21 + 0xa23*0xb31,
            0xa20*0xb02 + 0xa21*0xb12 + 0xa22*0xb22 + 0xa23*0xb32,
            0xa20*0xb03 + 0xa21*0xb13 + 0xa22*0xb23 + 0xa23*0xb33,
        },
        {
            0xa30*0xb00 + 0xa31*0xb10 + 0xa32*0xb20 + 0xa33*0xb30,
            0xa30*0xb01 + 0xa31*0xb11 + 0xa32*0xb21 + 0xa33*0xb31,
            0xa30*0xb02 + 0xa31*0xb12 + 0xa32*0xb22 + 0xa33*0xb32,
            0xa30*0xb03 + 0xa31*0xb13 + 0xa32*0xb23 + 0xa33*0xb33,
        },
    }}}
);

// --------------------------------------------------------------------------------------------------------------------
// scalar / linear
// --------------------------------------------------------------------------------------------------------------------

static_assert(2*3/vector_t<1>{3} == vector_t<1>{2});
static_assert(scalar_t{2*3}/vector_t<1>{3} == vector_t<1>{2});
static_assert(scalar_t{2*3*5}/vector_t<2>{3, 5} == vector_t<2>{2*5, 2*3});
static_assert(scalar_t{2*3}/matrix_t<1, 1>{{{{3}}}} == matrix_t<1, 1>{{{{2}}}});
static_assert(scalar_t{2*3*5}/matrix_t<1, 2>{{{{3, 5}}}} == matrix_t<1, 2>{{{{2*5, 2*3}}}});
static_assert(scalar_t{2*3*5}/matrix_t<2, 1>{{{{3}, {5}}}}  == matrix_t<2, 1>{{{{2*5}, {2*3}}}});
static_assert
(
    scalar_t{2*3*5*7*11}/matrix_t<2, 2>{{{{3, 5}, {7, 11}}}}
    ==
    matrix_t<2, 2>{{{{2*5*7*11, 2*3*7*11}, {2*3*5*11, 2*3*5*7}}}}
);

// --------------------------------------------------------------------------------------------------------------------
// linear / scalar
// --------------------------------------------------------------------------------------------------------------------

static_assert(vector_t<1>{2*3}/3 == vector_t<1>{2});

static_assert(vector_t<1>{2*3}/scalar_t{3} == vector_t<1>{2});
static_assert(vector_t<2>{2*3, 2*5}/scalar_t{2} == vector_t<2>{3, 5});
static_assert(matrix_t<1, 1>{{{{2*3}}}}/scalar_t{3} == matrix_t<1, 1>{{{{2}}}});
static_assert(matrix_t<1, 2>{{{{2*3, 2*5}}}}/scalar_t{2} == matrix_t<1, 2>{{{{3, 5}}}});
static_assert(matrix_t<2, 1>{{{{2*3}, {2*5}}}}/scalar_t{2} == matrix_t<2, 1>{{{{3}, {5}}}});
static_assert
(
    matrix_t<2, 2>{{{{2*3, 2*5}, {2*7, 2*11}}}}/scalar_t{2}
    ==
    matrix_t<2, 2>{{{{3, 5}, {7, 11}}}}
);

// clang-format on

} // namespace int_tests

// ====================================================================================================================
// float tests
// ====================================================================================================================

namespace float_tests {

// --------------------------------------------------------------------------------------------------------------------
// spot checks
// --------------------------------------------------------------------------------------------------------------------

using element_t                                  = float_t;
using scalar_t                                   = scalar_t<element_t>;
template <int_t size> using vector_t             = vector_t<float_t, size>;
template <int_t rows, int_t cols> using matrix_t = matrix_t<float_t, rows, cols>;

// clang-format off

static_assert(vector_t<2>{1.5, 2.5} + vector_t<2>{0.25, 0.75} == vector_t<2>{1.75, 3.25});
static_assert(vector_t<2>{3.0, 5.0} - vector_t<2>{1.0, 2.0} == vector_t<2>{2.0, 3.0});
static_assert(vector_t<2>{1.5, 2.5} * scalar_t{2.0} == vector_t<2>{3.0, 5.0});
static_assert(vector_t<2>{3.0, 6.0} / scalar_t{3.0} == vector_t<2>{1.0, 2.0});
static_assert(vector_t<2>{1.5, 2.5} * vector_t<2>{2.0, 4.0} == vector_t<2>{3.0, 10.0});
static_assert
(
    matrix_t<2, 2>{{{{1.0, 2.0}, {3.0, 4.0}}}} * matrix_t<2, 2>{{{{5.0, 6.0}, {7.0, 8.0}}}}
    ==
    matrix_t<2, 2>{{{{19.0, 22.0}, {43.0, 50.0}}}}
);

static_assert(matrix_t<2, 2>{{{{1.0, 2.0}, {3.0, 4.0}}}} * vector_t<2>{5.0, 6.0} == vector_t<2>{17.0, 39.0});

// --------------------------------------------------------------------------------------------------------------------
// mixed-type promotion: int op float -> float
// --------------------------------------------------------------------------------------------------------------------

static_assert(int_tests::vector_t<2>{1, 2} + vector_t<2>{0.5, 0.5} == vector_t<2>{1.5, 2.5});
static_assert(vector_t<2>{0.5, 0.5} + int_tests::vector_t<2>{1, 2} == vector_t<2>{1.5, 2.5});
static_assert(int_tests::vector_t<2>{3, 5} * scalar_t{0.5} == vector_t<2>{1.5, 2.5});
static_assert(scalar_t{0.5} * int_tests::vector_t<2>{3, 5} == vector_t<2>{1.5, 2.5});
static_assert(int_tests::vector_t<2>{3, 5} - vector_t<2>{0.5, 0.5} == vector_t<2>{2.5, 4.5});
static_assert
(
    int_tests::matrix_t<2, 2>{{{{1, 2}, {3, 4}}}} * matrix_t<2, 2>{{{{0.5, 1.5}, {2.5, 3.5}}}}
    ==
    matrix_t<2, 2>{{{{5.5, 8.5}, {11.5, 18.5}}}}
);

static_assert(int_tests::matrix_t<2, 2>{{{{1, 2}, {3, 4}}}} * vector_t<2>{0.5, 1.5} == vector_t<2>{3.5, 7.5});

// clang-format on

} // namespace float_tests

} // namespace
} // namespace crv::math
