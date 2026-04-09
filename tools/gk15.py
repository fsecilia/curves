#!/usr/bin/env python3
"""
Compute GK15 (Gauss-Kronrod 15-point) quadrature rule at float128 precision.

Nodes:
  The 15 Kronrod abscissas are the union of:
    - 7 roots of the Legendre polynomial P_7(x)
    - 8 roots of the Stieltjes polynomial E_8(x)

  The Stieltjes polynomial E_{n+1}(x) of degree n+1 is defined by:
      E_{n+1} monic
      ∫₋₁¹ E_{n+1}(x) · x^k · P_n(x) dx = 0   for k = 0, ..., n
  where P_n is the Legendre polynomial of degree n.

  Ref: Kronrod, A.S., "Nodes and Weights of Quadrature Formulas",
       Consultants Bureau, New York, 1965.
       Monegato, G., "Stieltjes polynomials and related quadrature rules",
       SIAM Review 24(2), 1982, pp. 137-158.

Weights:
  w_i = ∫₋₁¹ ℓ_i(x) dx  where ℓ_i is the Lagrange basis polynomial
  for node x_i among the full set of 15 (resp. 7) nodes.

  Ref: Davis & Rabinowitz, "Methods of Numerical Integration", §2.7.

Working precision: 200 decimal digits (mpmath).
Output:  hex-float with 113-bit significand (IEEE 754 binary128).

Usage:   python3 gk15_final.py
"""
import mpmath, sys

WORK_DPS = 200
mpmath.mp.dps = WORK_DPS

# ──────────────────────────────────────────────────────────────────────
# Stieltjes polynomial E_8 for the 7-point Gauss-Legendre rule
#
# For n=7, E_8 has degree 8.  By the symmetry of [-1,1] and the fact
# that P_7 is odd, the integrand E_8(x)·x^k·P_7(x) has parity
# (-1)^{8+k+7} = (-1)^{k+1}.  This is odd (hence integrates to 0
# automatically) when k is even.  The 4 nontrivial conditions come
# from k = 1, 3, 5, 7.
#
# E_8(x) = x^8 + c6·x^6 + c4·x^4 + c2·x^2 + c0
# ──────────────────────────────────────────────────────────────────────

def moment(m):
    """∫₋₁¹ x^m dx."""
    return mpmath.mpf(0) if m % 2 == 1 else mpmath.mpf(2) / (m + 1)

def legendre_mono_coeffs(n):
    """Monomial coefficients [c_0, ..., c_n] of the Legendre polynomial P_n(x)."""
    cs = [mpmath.mpf(0)] * (n + 1)
    for k in range(n // 2 + 1):
        p = n - 2 * k
        cs[p] = ((-1)**k * mpmath.fac(2*n - 2*k) /
                 (mpmath.power(2, n) * mpmath.fac(k) * mpmath.fac(n - k) * mpmath.fac(n - 2*k)))
    return cs

P7_coeffs = legendre_mono_coeffs(7)  # P_7(x) = Σ P7_coeffs[j] x^j

def int_xa_P7(a):
    """∫₋₁¹ x^a · P_7(x) dx  (exact via moments)."""
    return sum(c * moment(a + j) for j, c in enumerate(P7_coeffs) if c)

def int_xa_P7_E8_term(a, power):
    """∫₋₁¹ x^a · P_7(x) · x^power dx = int_xa_P7(a + power)."""
    return int_xa_P7(a + power)

# Build 4×4 system for c6, c4, c2, c0 from conditions at k = 1, 3, 5, 7:
#   ∫ E_8(x) · x^k · P_7(x) dx = 0
#   ∫ (x^8 + c6·x^6 + c4·x^4 + c2·x^2 + c0) · x^k · P_7(x) dx = 0
A = mpmath.matrix(4, 4)
b = mpmath.matrix(4, 1)
odd_ks = [1, 3, 5, 7]

for i, k in enumerate(odd_ks):
    for j, p in enumerate([6, 4, 2, 0]):
        A[i, j] = int_xa_P7(k + p)   # ∫ x^{k+p} P_7(x) dx
    b[i] = -int_xa_P7(k + 8)         # - ∫ x^{k+8} P_7(x) dx

sol = mpmath.lu_solve(A, b)
c6, c4, c2, c0 = sol[0], sol[1], sol[2], sol[3]

def E8(x):
    return x**8 + c6*x**6 + c4*x**4 + c2*x**2 + c0

# Verify orthogonality
for k in odd_ks:
    val = mpmath.quad(lambda x: E8(x) * x**k * mpmath.legendre(7, x), [-1, 1],
                      method='gauss-legendre', maxdegree=8)
    assert abs(val) < mpmath.power(10, -180), f"Orthogonality check failed for k={k}: {val}"

# ──────────────────────────────────────────────────────────────────────
# Find all 15 nodes
# ──────────────────────────────────────────────────────────────────────

# Positive roots of E_8 (4 of them — E_8 is even, degree 8)
# Scan for sign changes to get robust starting points
scan = [mpmath.mpf(i) / 10000 for i in range(1, 10000)]
brackets = []
for i in range(len(scan) - 1):
    if E8(scan[i]) * E8(scan[i+1]) < 0:
        brackets.append((scan[i], scan[i+1]))

assert len(brackets) == 4, f"Expected 4 positive root brackets, got {len(brackets)}"
kronrod_only_pos = sorted(mpmath.findroot(E8, (lo + hi) / 2) for lo, hi in brackets)

# Positive roots of P_7 (3 of them; x=0 is also a root since P_7 is odd)
P7 = lambda x: mpmath.legendre(7, x)
brackets_gl = []
for i in range(len(scan) - 1):
    if P7(scan[i]) * P7(scan[i+1]) < 0:
        brackets_gl.append((scan[i], scan[i+1]))

assert len(brackets_gl) == 3, f"Expected 3 positive GL7 root brackets, got {len(brackets_gl)}"
gl7_pos = sorted(mpmath.findroot(P7, (lo + hi) / 2) for lo, hi in brackets_gl)

# Full node sets
all_pos = sorted(gl7_pos + kronrod_only_pos)
nodes15 = [-x for x in reversed(all_pos)] + [mpmath.mpf(0)] + list(all_pos)
gl7_nodes = [-x for x in reversed(gl7_pos)] + [mpmath.mpf(0)] + list(gl7_pos)

# Tag shared nodes
gl7_indices = set()
gl7_ki_to_gi = {}
for gi, gn in enumerate(gl7_nodes):
    for ki, kn in enumerate(nodes15):
        if abs(gn - kn) < mpmath.power(10, -190):
            gl7_indices.add(ki)
            gl7_ki_to_gi[ki] = gi
            break

assert len(gl7_indices) == 7
assert len(nodes15) == 15

# ──────────────────────────────────────────────────────────────────────
# Compute weights
# ──────────────────────────────────────────────────────────────────────

def compute_weights(nlist):
    m = len(nlist)
    weights = []
    for i in range(m):
        denom = mpmath.fprod(nlist[i] - nlist[j] for j in range(m) if j != i)
        def integrand(x, _i=i, _d=denom):
            return mpmath.fprod(x - nlist[j] for j in range(m) if j != _i) / _d
        w = mpmath.quad(integrand, [-1, 1], method='gauss-legendre', maxdegree=8)
        weights.append(w)
    return weights

print("Computing Kronrod weights...", file=sys.stderr, flush=True)
kw = compute_weights(nodes15)
print("Computing Gauss weights...", file=sys.stderr, flush=True)
gw = compute_weights(gl7_nodes)

gl7_weight_at = {}
for ki, gi in gl7_ki_to_gi.items():
    gl7_weight_at[ki] = gw[gi]

# ──────────────────────────────────────────────────────────────────────
# Verification
# ──────────────────────────────────────────────────────────────────────
print("=" * 78)
print("GK15 Gauss-Kronrod Quadrature Rule")
print("=" * 78)
print()

kw_sum = mpmath.fsum(kw)
gw_sum = mpmath.fsum(gw)
print(f"Σ(Kronrod weights) = {mpmath.nstr(kw_sum, 60)}")
print(f"Σ(Gauss weights)   = {mpmath.nstr(gw_sum, 60)}")

# GK15 should integrate polynomials exactly up to degree 3n+1 = 22.
max_exact = 22
fail = False
for k in range(max_exact + 3):
    exact = moment(k)
    approx = mpmath.fsum(w * x**k for x, w in zip(nodes15, kw))
    err = abs(approx - exact)
    if k <= max_exact and err > mpmath.power(10, -150):
        print(f"  FAIL: moment k={k}, error = {mpmath.nstr(err, 8)}")
        fail = True
    elif k > max_exact and err > mpmath.power(10, -30):
        print(f"  k={k}: error = {mpmath.nstr(err, 8)}  (not exact here — expected)")

if not fail:
    print(f"Moments k=0..{max_exact}: exact to >{150} digits  ✓")
else:
    print("*** MOMENT VERIFICATION FAILED ***")
    sys.exit(1)

# Bonus: ∫₋₁¹ exp(x) dx = e − 1/e
exact_exp = mpmath.e - 1/mpmath.e
err_k = abs(mpmath.fsum(w * mpmath.exp(x) for x, w in zip(nodes15, kw)) - exact_exp)
err_g = abs(mpmath.fsum(w * mpmath.exp(x) for x, w in zip(gl7_nodes, gw)) - exact_exp)
print(f"∫exp(x): |GK15 err| = {mpmath.nstr(err_k, 6)},  |GL7 err| = {mpmath.nstr(err_g, 6)}")

# All weights must be positive for a valid Gauss-Kronrod rule
for i, w in enumerate(kw):
    if w <= 0:
        print(f"  ERROR: Kronrod weight[{i}] = {mpmath.nstr(w, 15)} is not positive!")
        fail = True
if not fail:
    print("All Kronrod weights positive  ✓")

# ──────────────────────────────────────────────────────────────────────
# Hex-float128
# ──────────────────────────────────────────────────────────────────────
def to_hex_f128(x):
    if x == 0:
        return " 0x0.0000000000000000000000000000p+0"
    sign = "-" if x < 0 else " "
    x = abs(x)
    man, exp = mpmath.frexp(x)
    man *= 2; exp -= 1
    int_man = int(mpmath.nint(man * mpmath.power(2, 112)))
    frac = int_man & ((1 << 112) - 1)
    return f"{sign}0x1.{frac:028x}p{exp:+d}"

# ──────────────────────────────────────────────────────────────────────
# Output
# ──────────────────────────────────────────────────────────────────────
print()
print("=" * 78)
print("Non-negative abscissas (symmetric rule: w(-x) = w(x) for all entries)")
print("=" * 78)
print()

nonneg = [(i, nodes15[i], kw[i]) for i in range(15) if nodes15[i] >= -mpmath.power(10, -190)]
nonneg.sort(key=lambda t: abs(t[1]))

print("// ---- Decimal (36 significant digits) ----")
for ki, x, wk in nonneg:
    tag = "G+K" if ki in gl7_indices else "  K"
    print(f"// [{tag}]  x  = {mpmath.nstr(abs(x), 36, strip_zeros=False)}")
    print(f"//        wK = {mpmath.nstr(wk, 36, strip_zeros=False)}")
    if ki in gl7_indices:
        print(f"//        wG = {mpmath.nstr(gl7_weight_at[ki], 36, strip_zeros=False)}")
    print("//")

print()
print("// ---- Hex-float128 (__float128 / _Float128 / Q suffix) ----")
print()
print("// Kronrod abscissas, non-negative (8 values; negate for the other 7):")
for ki, x, wk in nonneg:
    tag = "/* G+K */" if ki in gl7_indices else "/* K   */"
    print(f"    {to_hex_f128(abs(x))}Q,  {tag}")

print()
print("// Kronrod weights (same order as abscissas above):")
for ki, x, wk in nonneg:
    print(f"    {to_hex_f128(wk)}Q,")

print()
print("// Gauss weights (4 non-negative GL7 nodes only):")
for ki, x, wk in nonneg:
    if ki in gl7_indices:
        print(f"    {to_hex_f128(gl7_weight_at[ki])}Q,  // x ≈ {mpmath.nstr(abs(x), 12)}")
