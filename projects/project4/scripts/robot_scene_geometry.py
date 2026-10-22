"""LAS navigation evidence for robot/3DGS compositing and local ground height.

These checks use the existing scene obstacle contract. They are conservative
navigation geometry approximations, not a triangle-mesh depth buffer or a
physical scene simulation. No original scene coordinates are rescaled.
"""

from __future__ import annotations

import math
import time
from pathlib import Path
from typing import Any

import numpy as np
from scipy.spatial import cKDTree

from robotnav.navigation.scene.contracts import SceneObstacleModel


def _ray_is_supported(start_grid: np.ndarray, end_grid: np.ndarray, supported: np.ndarray) -> bool:
    """Visit every cell crossed by an X-Z segment, including corner neighbors."""
    height, width = supported.shape

    def contains(x: int, y: int) -> bool:
        return 0 <= x < width and 0 <= y < height and bool(supported[y, x])

    x, y = (int(value) for value in np.floor(start_grid))
    end_x, end_y = (int(value) for value in np.floor(end_grid))
    if not contains(x, y) or not contains(end_x, end_y):
        return False
    dx, dy = end_grid - start_grid
    step_x, step_y = int(np.sign(dx)), int(np.sign(dy))
    t_delta_x = abs(1 / dx) if step_x else math.inf
    t_delta_y = abs(1 / dy) if step_y else math.inf
    next_x = x + 1 if step_x > 0 else x
    next_y = y + 1 if step_y > 0 else y
    t_max_x = (next_x - start_grid[0]) / dx if step_x else math.inf
    t_max_y = (next_y - start_grid[1]) / dy if step_y else math.inf
    # Each update crosses at least one grid boundary; no unbounded iteration.
    for _ in range(abs(end_x - x) + abs(end_y - y) + 2):
        if x == end_x and y == end_y:
            return True
        if math.isclose(t_max_x, t_max_y, rel_tol=1e-10, abs_tol=1e-12):
            if not contains(x + step_x, y) or not contains(x, y + step_y):
                return False
            x += step_x
            y += step_y
            t_max_x += t_delta_x
            t_max_y += t_delta_y
        elif t_max_x < t_max_y:
            x += step_x
            t_max_x += t_delta_x
        else:
            y += step_y
            t_max_y += t_delta_y
        if not contains(x, y):
            return False
    return False


def filter_supported_occlusion(
    scene_depth: np.ndarray,
    scene_alpha: np.ndarray,
    robot_depth: np.ndarray,
    robot_mask: np.ndarray,
    optical_from_scene: np.ndarray,
    intrinsics: np.ndarray,
    model: SceneObstacleModel,
    *,
    alpha_threshold: float = 0.2,
    depth_tolerance: float = 0.025,
    height_min_m: float = -0.08,
    height_max_m: float = 0.65,
    clearance_m: float = 0.16,
) -> tuple[np.ndarray, dict[str, Any]]:
    """Reject only Gaussian occluders contradicted by checked LAS free space.

    An ED hit is rejected when its cell has actual ground evidence, is traversable,
    is sufficiently far from a physical obstacle, and its height lies within the
    specified navigation collision band. Every cell from the camera to that hit
    must also be known traversable and free of cleaned physical obstacles.
    Unknown, outside, boundary, elevated, and obstacle-supported hits retain the
    original depth decision.
    """
    shape = scene_depth.shape
    if any(array.shape != shape for array in (scene_alpha, robot_depth, robot_mask)):
        raise ValueError("Scene and robot depth/alpha/mask must have matching image shapes")
    if len(shape) != 2 or optical_from_scene.shape != (4, 4) or intrinsics.shape != (3, 3):
        raise ValueError("Expected 2D images, a 4x4 view matrix, and 3x3 intrinsics")
    if clearance_m < 0 or height_min_m > height_max_m:
        raise ValueError("Invalid geometry clearance or supported height interval")
    started = time.perf_counter()
    scene_valid = (scene_alpha >= alpha_threshold) & np.isfinite(scene_depth) & (scene_depth > 0)
    original_visible = robot_mask & (~scene_valid | (robot_depth <= scene_depth + depth_tolerance))
    candidates = robot_mask & ~original_visible
    count = int(np.count_nonzero(candidates))
    stats: dict[str, Any] = {
        "geometry_source": "LAS navigation ground and cleaned obstacle grid",
        "geometry_limit": "Navigation evidence approximation; not a triangle-mesh z-buffer",
        "candidate_occluded_pixels": count,
        "original_visible_robot_pixels": int(original_visible.sum()),
        "visible_robot_pixels": int(original_visible.sum()),
        "rejected_unsupported_gaussian_occluders": 0,
        "retained_occluded_pixels": count,
        "outside_scene_pixels": 0,
        "outside_height_interval_pixels": 0,
        "unsupported_endpoint_pixels": 0,
        "ray_blocked_or_unknown_pixels": 0,
        "height_interval_m": [height_min_m, height_max_m],
        "clearance_m": clearance_m,
        "depth_tolerance_m": depth_tolerance,
    }
    if not count:
        stats["elapsed_seconds"] = time.perf_counter() - started
        return original_visible, stats
    rows, columns = np.nonzero(candidates)
    pixels = np.column_stack((columns, rows, np.ones(count)))
    directions = pixels @ np.linalg.inv(intrinsics.astype(np.float64)).T
    optical_points = directions * (scene_depth[rows, columns] / directions[:, 2])[:, None]
    scene_from_optical = np.linalg.inv(optical_from_scene.astype(np.float64))
    points = optical_points @ scene_from_optical[:3, :3].T + scene_from_optical[:3, 3]
    heights = model.floor_y - points[:, 1]
    grid_coordinates = (points[:, [0, 2]] - model.origin_xz) / model.resolution_m
    cells = np.floor(grid_coordinates).astype(np.int64)
    map_height, map_width = model.cleaned_obstacles.shape
    inside = (
        (cells[:, 0] >= 0)
        & (cells[:, 0] < map_width)
        & (cells[:, 1] >= 0)
        & (cells[:, 1] < map_height)
    )
    in_height_band = (heights >= height_min_m) & (heights <= height_max_m)
    endpoints_supported = np.zeros(count, dtype=bool)
    indices = np.flatnonzero(inside)
    x, y = cells[indices].T
    endpoints_supported[indices] = (
        model.raw_ground[y, x]
        & model.traversable_ground[y, x]
        & ~model.cleaned_obstacles[y, x]
        & (model.raw_distance_m[y, x] >= clearance_m)
    )
    eligible = inside & in_height_band & endpoints_supported
    camera_xz = scene_from_optical[[0, 2], 3]
    start_grid = (camera_xz - model.origin_xz) / model.resolution_m
    ray_supported_grid = model.traversable_ground & ~model.cleaned_obstacles
    rejected = np.zeros(count, dtype=bool)
    for index in np.flatnonzero(eligible):
        rejected[index] = _ray_is_supported(start_grid, grid_coordinates[index], ray_supported_grid)
    visible = original_visible.copy()
    visible[rows[rejected], columns[rejected]] = True
    stats.update(
        {
            "candidate_height_quantiles_m": np.percentile(heights, [0, 5, 50, 95, 100]).tolist(),
            "outside_scene_pixels": int((~inside).sum()),
            "outside_height_interval_pixels": int((inside & ~in_height_band).sum()),
            "unsupported_endpoint_pixels": int(
                (inside & in_height_band & ~endpoints_supported).sum()
            ),
            "ray_blocked_or_unknown_pixels": int((eligible & ~rejected).sum()),
            "rejected_unsupported_gaussian_occluders": int(rejected.sum()),
            "retained_occluded_pixels": int(count - rejected.sum()),
            "visible_robot_pixels": int(visible.sum()),
            "elapsed_seconds": time.perf_counter() - started,
        }
    )
    return visible, stats


def estimate_ground_profile(
    ground_path: Path,
    points_xz: np.ndarray,
    floor_y: float,
    axis_transform: str,
    *,
    band_m: float = 0.08,
    radius_m: float = 0.25,
    voxel_size_m: float = 0.04,
) -> tuple[np.ndarray, dict[str, Any]]:
    """Estimate local floor offsets from bundled real LAS representatives.

    The preparation step retained the nearest actual LAS point to each 4cm voxel
    center within the laboratory floor band. These records are bundled locally.
    The local profile is the median of representatives within each planar radius.
    Missing support fails explicitly instead of placing feet on an assumed plane.
    """
    if points_xz.ndim != 2 or points_xz.shape[1] != 2 or len(points_xz) < 1:
        raise ValueError("points_xz must be a nonempty (N,2) route")
    if not np.isfinite(points_xz).all() or not np.isfinite(floor_y):
        raise ValueError("Route and floor height must be finite")
    if min(band_m, radius_m, voxel_size_m) <= 0:
        raise ValueError("Ground band, radius, and voxel size must be positive")
    started = time.perf_counter()
    with np.load(ground_path, allow_pickle=False) as data:
        representatives = np.asarray(data["points_xzh"], dtype=np.float64)
    if representatives.ndim != 2 or representatives.shape[1] != 3:
        raise ValueError("Bundled ground support must have shape (N,3)")
    if not np.isfinite(representatives).all():
        raise ValueError("Bundled ground support contains non-finite values")
    tree = cKDTree(representatives[:, :2])
    neighborhoods = tree.query_ball_point(points_xz, radius_m)
    unsupported = [index for index, neighbors in enumerate(neighborhoods) if not len(neighbors)]
    if unsupported:
        raise ValueError(
            f"LAS local floor has no support within {radius_m:.3f} m at route indices "
            f"{unsupported[:12]} ({len(unsupported)} points total)"
        )
    local_heights = np.array(
        [np.median(representatives[neighbors, 2]) for neighbors in neighborhoods], dtype=np.float64
    )
    report = {
        "geometry_source": "Bundled actual voxel representatives extracted from original LAS",
        "geometry_limit": "Local median ground-height approximation; not a surface mesh or physics",
        "ground_support_path": str(ground_path),
        "axis_transform": axis_transform,
        "floor_y": float(floor_y),
        "height_convention": "floor_y - worldY",
        "band_m": band_m,
        "radius_m": radius_m,
        "voxel_size_m": voxel_size_m,
        "ground_voxel_representatives": len(representatives),
        "route_point_count": len(points_xz),
        "unsupported_route_points": 0,
        "neighbor_count_quantiles": np.percentile(
            [len(ids) for ids in neighborhoods], [0, 50, 100]
        ).tolist(),
        "local_height_quantiles_m": np.percentile(local_heights, [0, 5, 50, 95, 100]).tolist(),
        "elapsed_seconds": time.perf_counter() - started,
    }
    return local_heights, report
