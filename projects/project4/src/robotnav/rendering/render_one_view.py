import math

import numpy as np
import torch
import torch.nn.functional as F
from gsplat import rasterization
from plyfile import PlyData

SH_C0 = 0.28209479177387814


def decode_ply_sh_coefficients(f_dc: np.ndarray, f_rest: np.ndarray, sh_degree: int) -> np.ndarray:
    """Decode Inria 3DGS PLY SH properties to gsplat's ``[N, K, RGB]`` layout.

    The reference 3DGS exporter serializes ``f_rest_*`` by colour channel:
    all R coefficients, then all G coefficients, then all B coefficients.  In
    contrast, gsplat expects every SH basis to contain an RGB triplet.  Keeping
    the PLY flat order and simply reshaping to ``[N, K, 3]`` mixes coefficients
    across channels, which creates view-dependent magenta/green artifacts.
    """
    expected_rest_coeffs = (sh_degree + 1) ** 2 - 1
    if f_rest.ndim != 2 or f_rest.shape[1] != expected_rest_coeffs * 3:
        raise ValueError(
            "PLY f_rest layout does not match the requested SH degree: "
            f"degree={sh_degree}, f_rest shape={f_rest.shape}"
        )
    rest_by_channel = f_rest.reshape(f_rest.shape[0], 3, expected_rest_coeffs)
    rest_by_basis = np.moveaxis(rest_by_channel, 1, 2)
    return np.concatenate([f_dc[:, None, :], rest_by_basis], axis=1)


def report_rgb_range(label: str, rgb: torch.Tensor) -> None:
    """Print renderer RGB ranges before the PNG-specific [0, 1] clamp.

    3DGS colors are display-referred values learned from the input images, not
    scene-linear radiance.  The report makes the only PNG conversion explicit:
    clamp to [0, 1], then quantize to uint8; no extra tone map or gamma curve is
    applied.
    """
    below = int((rgb < 0.0).sum().item())
    above = int((rgb > 1.0).sum().item())
    print(
        f"{label} RGB before PNG clamp: {rgb.min().item():.6g}..{rgb.max().item():.6g}; "
        f"outside [0,1]: below={below}, above={above}"
    )


def display_rgb_to_uint8(rgb: torch.Tensor) -> np.ndarray:
    """Quantize display-referred 3DGS RGB for PNG storage."""
    return (rgb.clamp(0.0, 1.0).detach().cpu().numpy() * 255.0).round().astype(np.uint8)


def _rasterize_compat(**kwargs):
    """Call gsplat with the current background shape, then support older APIs."""
    backgrounds = kwargs.get("backgrounds")
    try:
        return rasterization(**kwargs)
    except AssertionError as exc:
        if backgrounds is None:
            raise
        fallback = dict(kwargs)
        if backgrounds.ndim == 1:
            camera_count = kwargs["viewmats"].shape[-3]
            fallback["backgrounds"] = backgrounds.unsqueeze(0).expand(camera_count, -1)
        else:
            fallback["backgrounds"] = backgrounds[0]
        try:
            return rasterization(**fallback)
        except AssertionError as fallback_error:
            raise exc from fallback_error


def _sorted_property_names(vertex, prefix):
    names = [p.name for p in vertex.properties if p.name.startswith(prefix)]
    return sorted(names, key=lambda name: int(name.split("_")[-1]))


def _require_properties(vertex, names):
    available = {p.name for p in vertex.properties}
    missing = [name for name in names if name not in available]
    if missing:
        raise ValueError(f"PLY is missing required 3DGS properties: {missing}")


def load_ply_to_torch(
    ply_path, device="cuda", unit_scale=1.0, max_gaussians=0, sh_degree_arg="auto"
):
    print(f"Reading 3DGS PLY: {ply_path}")
    plydata = PlyData.read(ply_path)
    vertex = plydata["vertex"]

    _require_properties(
        vertex,
        [
            "x",
            "y",
            "z",
            "f_dc_0",
            "f_dc_1",
            "f_dc_2",
            "opacity",
            "scale_0",
            "scale_1",
            "scale_2",
            "rot_0",
            "rot_1",
            "rot_2",
            "rot_3",
        ],
    )

    xyz = np.stack([vertex["x"], vertex["y"], vertex["z"]], axis=-1).astype(np.float32)
    scale_names = _sorted_property_names(vertex, "scale_")
    rot_names = _sorted_property_names(vertex, "rot_")
    rest_names = _sorted_property_names(vertex, "f_rest_")

    scales_np = np.stack([vertex[name] for name in scale_names], axis=-1).astype(np.float32)
    quats_np = np.stack([vertex[name] for name in rot_names], axis=-1).astype(np.float32)
    opacity_np = np.asarray(vertex["opacity"], dtype=np.float32)
    f_dc = np.stack([vertex["f_dc_0"], vertex["f_dc_1"], vertex["f_dc_2"]], axis=-1).astype(
        np.float32
    )
    f_rest = None
    if rest_names:
        f_rest = np.stack([vertex[name] for name in rest_names], axis=-1).astype(np.float32)

    total = xyz.shape[0]
    if max_gaussians and total > max_gaussians:
        rng = np.random.default_rng(0)
        keep = np.sort(rng.choice(total, size=max_gaussians, replace=False))
        xyz = xyz[keep]
        scales_np = scales_np[keep]
        quats_np = quats_np[keep]
        opacity_np = opacity_np[keep]
        f_dc = f_dc[keep]
        if f_rest is not None:
            f_rest = f_rest[keep]
        print(f"Subsampled {total} -> {len(keep)} gaussians for a quick render.")

    means = torch.from_numpy(xyz).to(device) * unit_scale
    scales = torch.exp(torch.from_numpy(scales_np).to(device)) * unit_scale
    quats = F.normalize(torch.from_numpy(quats_np).to(device), dim=-1)
    opacities = torch.sigmoid(torch.from_numpy(opacity_np).to(device))

    sh_degree = None
    if sh_degree_arg.lower() == "dc":
        colors = torch.clamp(torch.from_numpy(f_dc).to(device) * SH_C0 + 0.5, 0.0, 1.0)
    else:
        max_sh_degree = 0
        if f_rest is not None and f_rest.shape[1] % 3 == 0:
            coeff_count = 1 + f_rest.shape[1] // 3
            root = int(math.sqrt(coeff_count))
            if root * root == coeff_count:
                max_sh_degree = root - 1

        requested = max_sh_degree if sh_degree_arg.lower() == "auto" else int(sh_degree_arg)
        sh_degree = min(requested, max_sh_degree)
        if sh_degree < 0:
            sh_degree = None
            colors = torch.clamp(torch.from_numpy(f_dc).to(device) * SH_C0 + 0.5, 0.0, 1.0)
        elif sh_degree == 0:
            colors = torch.from_numpy(f_dc[:, None, :]).to(device)
        else:
            sh_coeffs = decode_ply_sh_coefficients(f_dc, f_rest, sh_degree)
            colors = torch.from_numpy(sh_coeffs).to(device)

    bbox_min = means.min(dim=0).values
    bbox_max = means.max(dim=0).values
    center = (bbox_min + bbox_max) * 0.5
    extent = bbox_max - bbox_min
    radius = torch.linalg.norm(extent) * 0.5

    print(f"Loaded gaussians: {means.shape[0]}")
    print(f"Scene center: {center.detach().cpu().numpy()}")
    print(f"Scene extent: {extent.detach().cpu().numpy()} (unit_scale={unit_scale})")
    print(f"Scale range: {scales.min().item():.6g} .. {scales.max().item():.6g}")
    print(f"Opacity range: {opacities.min().item():.6g} .. {opacities.max().item():.6g}")
    print(f"Color mode: {'DC RGB' if sh_degree is None else f'SH degree {sh_degree}'}")

    return means, scales, quats, opacities, colors, center, radius, sh_degree
