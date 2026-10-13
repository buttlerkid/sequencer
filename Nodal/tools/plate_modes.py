"""
Free-edge Kirchhoff plate modes for DY Nodal's 2D bodies.

    D * del^4 w = rho*h * omega^2 * w,  all edges free (no supports), Poisson ratio NU

Method: Rayleigh-Ritz with a tensor Legendre basis p_i(x/sx) p_j(y/sy) over the
plate's bounding box, integrated over the actual plate shape. Free edges are the
natural boundary condition of the energy functional, so any shape works with no
constraints. The three rigid-body motions come out at (numerically) zero and are
dropped.

Checks (printed, and the run fails if they are off):
  * square: Leissa's free square plate, nu = 0.3, omega*a^2*sqrt(rho h/D)
  * circle: the exact solution w = [J_n(k r) + C I_n(k r)] cos(n theta) with the
            free-edge moment and shear conditions, solved here independently

Writes Source/Engine/PlateModes.inc (coefficients, frequency parameters, labels).
Run from the Nodal folder:  python tools/plate_modes.py
"""
import numpy as np
from numpy.polynomial import legendre as leg
from scipy.linalg import eigh
from scipy.special import jv, jvp, iv, ivp
from scipy.optimize import brentq
import pathlib, sys

NU = 0.3
DEG = 22                    # Legendre degree per axis
NB = DEG + 1
COUNT = 48                  # modes kept per body (kModeTable)
VIOLIN_HW = 0.62

# ------------------------------------------------------------------ shapes (mirror Bodies.cpp insidePlate)
def in_ellipse(x, y, cx, cy, rx, ry):
    return ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1.0

def inside(body, x, y):
    if body == "square":
        return (np.abs(x) <= 1) & (np.abs(y) <= 1)
    if body == "circle":
        return x * x + y * y <= 1
    if body == "hexagon":
        ax, ay = np.abs(x), np.abs(y)
        return (ay <= 0.8660254) & (1.7320508 * ax + ay <= 1.7320508)
    if body == "violin":
        b = in_ellipse(x, y, 0, -0.42, VIOLIN_HW, 0.54) | in_ellipse(x, y, 0, 0.47, 0.50, 0.46) | ((np.abs(x) < 0.42) & (np.abs(y) < 0.25))
        c = 0.07
        cut = ((x - 0.64) ** 2 + (y - 0.03) ** 2 < c) | ((x + 0.64) ** 2 + (y - 0.03) ** 2 < c)
        return b & ~cut
    raise ValueError(body)

BOX = {"square": (1.0, 1.0), "circle": (1.0, 1.0), "hexagon": (1.0, 0.8660254), "violin": (VIOLIN_HW, 1.0)}

# ------------------------------------------------------------------ basis
def basis(t):
    """Normalised Legendre p_i(t) = sqrt((2i+1)/2) P_i(t) and its first two derivatives, shape (len(t), NB)."""
    out = []
    for d in range(3):
        cols = []
        for i in range(NB):
            c = np.zeros(NB); c[i] = np.sqrt((2 * i + 1) / 2.0)
            cols.append(leg.legval(t, leg.legder(c, d) if d else c))
        out.append(np.stack(cols, axis=1))
    return out

def quadrature(body):
    """Nodes along each axis (in box units t, s in [-1, 1]) and a weight matrix covering the plate."""
    sx, sy = BOX[body]
    if body == "square":
        t, w = leg.leggauss(80)
        return t, t, np.outer(w, w)
    n, sub = 700, 4                                # cells per axis, subsamples per cell side for edge coverage
    edges = np.linspace(-1, 1, n + 1)
    centers = 0.5 * (edges[:-1] + edges[1:])
    fine = np.linspace(-1, 1, n * sub, endpoint=False) + 1.0 / (n * sub)
    X, Y = np.meshgrid(fine * sx, fine * sy, indexing="ij")
    cover = inside(body, X, Y).reshape(n, sub, n, sub).mean(axis=(1, 3))
    h = 2.0 / n
    return centers, centers, cover * h * h

def ritz(body):
    sx, sy = BOX[body]
    tx, ty, W = quadrature(body)
    Px = basis(tx); Py = basis(ty)
    Px = [Px[0], Px[1] / sx, Px[2] / sx ** 2]        # chain rule: d/dx = (1/sx) d/dt
    Py = [Py[0], Py[1] / sy, Py[2] / sy ** 2]

    def integral(a, b, c, d):
        # sum_xy W(x,y) * A_i(x) B_k(x) * C_j(y) D_l(y), arranged as [(i,j), (k,l)]
        Fx = (Px[a][:, :, None] * Px[b][:, None, :]).reshape(len(tx), NB * NB)    # [x, (i,k)]
        Fy = (Py[c][:, :, None] * Py[d][:, None, :]).reshape(len(ty), NB * NB)    # [y, (j,l)]
        G = Fx.T @ W @ Fy                                                          # [(i,k), (j,l)]
        return G.reshape(NB, NB, NB, NB).transpose(0, 2, 1, 3).reshape(NB * NB, NB * NB)

    K = (integral(2, 2, 0, 0) + integral(0, 0, 2, 2)
         + NU * (integral(2, 0, 0, 2) + integral(0, 2, 2, 0))
         + 2 * (1 - NU) * integral(1, 1, 1, 1))
    M = integral(0, 0, 0, 0)
    K = 0.5 * (K + K.T); M = 0.5 * (M + M.T)
    # restrict to the well-conditioned part of the basis on this domain
    s, U = np.linalg.eigh(M)
    keep = s > s.max() * 1e-11
    T = U[:, keep] / np.sqrt(s[keep])
    lam, V = np.linalg.eigh(T.T @ K @ T)
    C = T @ V                                       # M-orthonormal coefficient vectors
    return lam, C

# ------------------------------------------------------------------ evaluation
def evaluate(body, coeff, x, y):
    sx, sy = BOX[body]
    px = basis(np.asarray(x).ravel() / sx)[0]; py = basis(np.asarray(y).ravel() / sy)[0]
    c = coeff.reshape(NB, NB)
    return np.einsum("ni,ij,nj->n", px, c, py).reshape(np.shape(x))

def symmetrise_pairs(body, lam, C, idx):
    """Rotate each degenerate pair into the members that are even / odd under the body's mirror,
    so their figures look like the textbook ones (any rotation is equally valid physically)."""
    out = [C[:, i].copy() for i in idx]
    sign_y = np.array([(-1) ** j for i in range(NB) for j in range(NB)], float)
    k = 0
    while k < len(idx) - 1:
        a, b = idx[k], idx[k + 1]
        if abs(lam[b] / lam[a] - 1) < 2e-3:
            u, v = out[k], out[k + 1]
            if body == "square":
                S = lambda c: c.reshape(NB, NB).T.ravel()          # swap x and y
            else:
                S = lambda c: c * sign_y                            # mirror y -> -y
            A = np.array([[u @ S(u), u @ S(v)], [v @ S(u), v @ S(v)]])
            w, R = np.linalg.eigh(0.5 * (A + A.T))
            out[k], out[k + 1] = R[0, 1] * u + R[1, 1] * v, R[0, 0] * u + R[1, 0] * v   # even first
            k += 2
        else:
            k += 1
    return out

# ------------------------------------------------------------------ exact circle (reference)
def circle_bc(n, k):
    """2x2 determinant of the free-edge conditions (radial moment, Kelvin-Kirchhoff shear) at r = 1."""
    def parts(f, fp, fpp, fppp, sign):
        # radial function R(r) = f(k r); derivatives at r = 1
        R, R1, R2, R3 = f, k * fp, k * k * fpp, k ** 3 * fppp
        Mr = R2 + NU * (R1 - n * n * R)
        lap = sign * k * k                                   # del^2 of J is -k^2 J, of I is +k^2 I
        Vr = lap * R1 - (1 - NU) * n * n * (R1 - R)
        return Mr, Vr
    J = [jvp(n, k, d) if d else jv(n, k) for d in range(4)]
    I = [ivp(n, k, d) if d else iv(n, k) for d in range(4)]
    mJ, vJ = parts(*J, -1.0)
    mI, vI = parts(*I, +1.0)
    return mJ * vI - mI * vJ

def circle_exact(nmax=10, kmax=13.0):
    roots = []
    for n in range(nmax + 1):
        ks = np.linspace(0.3, kmax, 4000)
        f = [circle_bc(n, k) for k in ks]
        for a, b, fa, fb in zip(ks[:-1], ks[1:], f[:-1], f[1:]):
            if np.sign(fa) != np.sign(fb):
                k = brentq(lambda z: circle_bc(n, z), a, b)
                if not (n <= 1 and k < 0.5):               # rigid motions
                    roots.append((k * k, n))
    return sorted(roots)

# ------------------------------------------------------------------ labels
def classify_circle(coeff, body="circle", rr=0.75, rmax=0.99):
    th = np.linspace(0, 2 * np.pi, 360, endpoint=False)
    ring = evaluate(body, coeff, rr * np.cos(th), rr * np.sin(th))
    spec = np.abs(np.fft.rfft(ring))
    n = int(np.argmax(spec[:12]))
    phase = np.angle(np.fft.rfft(ring)[n]) if n else 0.0
    th0 = -phase / n if n else 0.0
    r = np.linspace(0.0, rmax, 400)
    prof = evaluate(body, coeff, r * np.cos(th0), r * np.sin(th0))
    big = np.abs(prof) > 0.02 * np.abs(prof).max()          # ignore the flat centre of high-n modes
    sg = np.sign(prof[big])
    rings = int(np.sum(sg[1:] != sg[:-1]))
    return n, rings, "%d diam · %d ring%s" % (n, rings, "" if rings == 1 else "s")

def classify_cos(body, coeff, mask_xy):
    """Nearest textbook Chladni figure: correlation with cos(a pi u) cos(b pi v) (+/- the swapped term)."""
    X, Y = mask_xy
    w = evaluate(body, coeff, X, Y)
    sx, sy = BOX[body]
    u, v = (X / sx + 1) / 2, (Y / sy + 1) / 2
    best = (0, 0, 0, "")
    for a in range(0, 10):
        for b in range(a if body == "square" else 0, 12):
            for sgn in ((+1, -1) if (body == "square" and a != b) else (0,)):
                f = np.cos(a * np.pi * u) * np.cos(b * np.pi * v)
                if sgn: f = f + sgn * np.cos(b * np.pi * u) * np.cos(a * np.pi * v)
                nf = np.linalg.norm(f)
                if nf < 1e-9: continue
                cor = abs(w @ f) / (np.linalg.norm(w) * nf)
                if cor > best[0]: best = (cor, a, b, "+" if sgn > 0 else "−" if sgn < 0 else "")
    cor, a, b, s = best
    if cor < 0.85: return None
    lab = "N %d · M %d" % (a, b) + ((" " + s) if s else "")
    return lab

# ------------------------------------------------------------------ main
def build(body):
    lam, C = ritz(body)
    rigid = lam[:3]
    assert np.all(np.abs(rigid) < lam[3] * 1e-3), (body, rigid, lam[3])
    lam, C = lam[3:], C[:, 3:]
    idx = list(range(COUNT + 1))
    vecs = symmetrise_pairs(body, lam, C, idx)[:COUNT]
    omega = np.sqrt(lam[:COUNT])                     # frequency parameter omega * sqrt(rho h / D), box units

    # evaluation grid inside the plate, for normalisation and labels
    sx, sy = BOX[body]
    g = np.linspace(-1, 1, 241)
    GX, GY = np.meshgrid(g * sx, g * sy, indexing="ij")
    m = inside(body, GX, GY)
    pts = (GX[m], GY[m])
    modes = []
    for k, c in enumerate(vecs):
        w = evaluate(body, c, *pts)
        if w[np.argmax(np.abs(w))] < 0: c = -c; w = -w
        # equal modal mass: every mode gets the same RMS over the plate (0.5), so a point force
        # excites each one by its shape value there, as in the real plate (corners swing harder)
        norm = 0.5 / np.sqrt(np.mean(w * w))
        n = rings = -1
        if body == "circle":
            n, rings, label = classify_circle(c)
        elif body == "hexagon":
            n, rings, label = classify_circle(c, "hexagon", 0.6, 0.84)
        else:
            label = classify_cos(body, c, pts) or ("mode %d" % (k + 1))
        modes.append(dict(omega=omega[k], norm=norm, label=label, coeff=c * norm, n=n, rings=rings))
    # identity for the display: modes that are one degenerate pair share a group number
    grp = 0
    for k, md in enumerate(modes):
        if k > 0 and abs(md["omega"] / modes[k - 1]["omega"] - 1) < 2e-3:
            prev = modes[k - 1]                          # one degenerate pair: one figure, turned
            md.update(group=prev["group"], variant=1, label=prev["label"], n=prev["n"], rings=prev["rings"])
        else: grp += 1; md["group"] = grp; md["variant"] = 0
    return modes

def check(modes_by_body):
    ok = True
    sq = [m["omega"] * 4.0 for m in modes_by_body["square"][:8]]      # side a = 2: omega a^2 = 4 omega
    leissa = [13.468, 19.596, 24.270, 34.801, 34.801, 61.093, 61.093, 63.686]
    print("square vs Leissa (nu 0.3):")
    for got, ref in zip(sq, leissa):
        e = got / ref - 1
        print("   %8.3f  %8.3f  %+.3f %%" % (got, ref, 100 * e)); ok &= abs(e) < 0.005
    ex = circle_exact()
    print("circle vs exact Kirchhoff (nu %.2f):" % NU)
    cm = modes_by_body["circle"]
    j = 0
    for k, md in enumerate(cm[:24]):
        if md.get("variant") == 1: continue
        ref, n = ex[j]; j += 1
        e = md["omega"] / ref - 1
        print("   %8.3f  %8.3f  n=%d  %-22s %+.3f %%" % (md["omega"], ref, n, md["label"], 100 * e)); ok &= abs(e) < 0.005
    return ok

if __name__ == "__main__":
    bodies = ["square", "circle", "hexagon", "violin"]
    allm = {b: build(b) for b in bodies}
    if not check(allm):
        print("REFERENCE CHECK FAILED"); sys.exit(1)
    for b in ("hexagon", "violin"):
        print(b, " ".join("%.2f" % (m["omega"] / allm[b][0]["omega"]) for m in allm[b][:12]))

    out = pathlib.Path(__file__).resolve().parent.parent / "Source" / "Engine" / "PlateModes.inc"
    L = ["// Generated by tools/plate_modes.py - do not edit.",
         "// Free-edge Kirchhoff plate modes (nu = %.2f), Rayleigh-Ritz, Legendre degree %d per axis." % (NU, DEG),
         "// Shape(x, y) = sum_ij c[i*%d+j] p_i(x/sx) p_j(y/sy), p_i = sqrt((2i+1)/2) P_i; RMS over the plate = 0.5." % NB,
         "#pragma once", "#ifdef _MSC_VER", "#pragma warning (push)", "#pragma warning (disable : 4305) // double literals into float tables", "#endif", "namespace dy::nodal::plate {",
         "constexpr int kDegree = %d, kBasis = %d, kCount = %d;" % (DEG, NB, COUNT),
         "struct ModeInfo { double omega; int group, variant, n, rings; const char* label; };",
         "struct Table { float sx, sy; const ModeInfo* modes; const float* coeffs; };"]
    for b in bodies:
        L.append("inline const ModeInfo %sModes[kCount] = {" % b)
        for md in allm[b]:
            esc = "".join(ch if ord(ch) < 128 else "".join("\\%03o" % x for x in ch.encode("utf-8")) for ch in md["label"])
            L.append('    { %.9g, %d, %d, %d, %d, "%s" },' % (md["omega"], md["group"], md["variant"], md["n"], md["rings"], esc))
        L.append("};")
        L.append("inline const float %sCoeffs[kCount * kBasis * kBasis] = {" % b)
        for md in allm[b]:
            L.append("    " + ",".join("%.7g" % v for v in md["coeff"]) + ",")
        L.append("};")
    for b in bodies:
        sx, sy = BOX[b]
        L.append("inline const Table %s { %.7ff, %.7ff, %sModes, %sCoeffs };" % (b, sx, sy, b, b))
    L.append("} // namespace dy::nodal::plate")
    L += ["#ifdef _MSC_VER", "#pragma warning (pop)", "#endif"]
    out.write_text("\n".join(L) + "\n", encoding="utf-8")
    print("wrote", out, "%.1f MB" % (out.stat().st_size / 1e6))
